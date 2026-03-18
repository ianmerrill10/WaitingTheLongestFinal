import { createRescueGroupsClient } from "./client";
import type { RGIncludedItem, RGOrgAttributes } from "./client";
import { mapRescueGroupsDog, extractPhotoUrls } from "./mapper";
import { createAdminClient } from "@/lib/supabase/admin";

interface SyncResult {
  totalFetched: number;
  inserted: number;
  updated: number;
  sheltersCreated: number;
  errors: number;
  duration: number;
}

const VALID_STATES = new Set([
  "AL","AK","AZ","AR","CA","CO","CT","DE","FL","GA",
  "HI","ID","IL","IN","IA","KS","KY","LA","ME","MD",
  "MA","MI","MN","MS","MO","MT","NE","NV","NH","NJ",
  "NM","NY","NC","ND","OH","OK","OR","PA","RI","SC",
  "SD","TN","TX","UT","VT","VA","WA","WV","WI","WY","DC",
]);

function mapOrgType(type?: string): string {
  if (!type) return "rescue";
  const lower = type.toLowerCase();
  if (lower === "municipal") return "municipal";
  if (lower.includes("shelter")) return "private";
  if (lower.includes("humane")) return "humane_society";
  if (lower.includes("spca")) return "spca";
  if (lower.includes("foster")) return "foster_network";
  return "rescue";
}

async function findOrCreateShelter(
  supabase: ReturnType<typeof createAdminClient>,
  orgId: string,
  included: RGIncludedItem[],
  shelterCache: Map<string, string>
): Promise<string | null> {
  if (shelterCache.has(orgId)) {
    return shelterCache.get(orgId)!;
  }

  // Check DB by external_id
  const { data: existing } = await supabase
    .from("shelters")
    .select("id")
    .eq("external_source", "rescuegroups")
    .eq("external_id", orgId)
    .single();

  if (existing) {
    shelterCache.set(orgId, existing.id);
    return existing.id;
  }

  // Find org in included data
  const orgItem = included.find((i) => i.type === "orgs" && i.id === orgId);
  if (!orgItem) return null;

  const org = orgItem.attributes as unknown as RGOrgAttributes;
  const stateCode = (org.state || "").toUpperCase();
  if (!VALID_STATES.has(stateCode) || !org.name || !org.city) return null;

  const shelterName = org.name.substring(0, 200);

  // Try to insert
  const { data: newShelter, error } = await supabase
    .from("shelters")
    .insert({
      name: shelterName,
      city: org.city.substring(0, 100),
      state_code: stateCode,
      zip_code: org.postalcode || null,
      phone: org.phone || null,
      email: org.email || null,
      website: org.url ? org.url.substring(0, 500) : null,
      facebook_url: org.facebookUrl ? org.facebookUrl.substring(0, 500) : null,
      description: org.about ? org.about.substring(0, 5000) : null,
      shelter_type: mapOrgType(org.type),
      latitude: org.lat || null,
      longitude: org.lon || null,
      external_id: orgId,
      external_source: "rescuegroups",
      is_verified: true,
    })
    .select("id")
    .single();

  if (error) {
    // Duplicate name+state — find existing and link
    const { data: byName } = await supabase
      .from("shelters")
      .select("id")
      .eq("name", shelterName)
      .eq("state_code", stateCode)
      .single();

    if (byName) {
      await supabase
        .from("shelters")
        .update({ external_id: orgId, external_source: "rescuegroups" })
        .eq("id", byName.id);
      shelterCache.set(orgId, byName.id);
      return byName.id;
    }
    return null;
  }

  shelterCache.set(orgId, newShelter.id);
  return newShelter.id;
}

/**
 * Sync dogs from RescueGroups v5 API to Supabase.
 * Uses batch operations for performance.
 */
export async function syncDogsFromRescueGroups(
  maxPages: number = 10,
  startPage: number = 1
): Promise<SyncResult> {
  const startTime = Date.now();
  const client = createRescueGroupsClient();
  const supabase = createAdminClient();

  let totalFetched = 0;
  let inserted = 0;
  let updated = 0;
  let sheltersCreated = 0;
  let errors = 0;

  const shelterCache = new Map<string, string>();
  const endPage = startPage + maxPages - 1;

  for (let page = startPage; page <= endPage; page++) {
    try {
      const response = await client.fetchAnimals(page, 100);

      if (!response.data || response.data.length === 0) break;

      const included = response.included || [];
      totalFetched += response.data.length;

      // Batch: get all external IDs for this page
      const externalIds = response.data.map((a) => a.id);

      // Batch lookup: find all existing dogs by external_id in one query
      const { data: existingDogs } = await supabase
        .from("dogs")
        .select("id, external_id")
        .eq("external_source", "rescuegroups")
        .in("external_id", externalIds);

      const existingMap = new Map(
        (existingDogs || []).map((d) => [d.external_id, d.id])
      );

      // Resolve all shelters for this page first
      const uniqueOrgIds = Array.from(
        new Set(
          response.data
            .map((a) => a.relationships?.orgs?.data?.[0]?.id)
            .filter((id): id is string => !!id)
        )
      );

      const prevCacheSize = shelterCache.size;
      for (const orgId of uniqueOrgIds) {
        await findOrCreateShelter(supabase, orgId, included, shelterCache);
      }
      sheltersCreated += shelterCache.size - prevCacheSize;

      // Prepare batch inserts and updates
      const toInsert: Record<string, unknown>[] = [];
      const toUpdate: { id: string; data: Record<string, unknown> }[] = [];

      for (const animal of response.data) {
        try {
          const pictureIds =
            animal.relationships?.pictures?.data?.map((p) => p.id) || [];
          const photoData = extractPhotoUrls(pictureIds, included);
          const mapped = mapRescueGroupsDog(
            animal.id,
            animal.attributes,
            photoData
          );

          const orgId = animal.relationships?.orgs?.data?.[0]?.id;
          const shelterId = orgId ? shelterCache.get(orgId) : null;

          if (!shelterId) {
            errors++;
            continue;
          }

          const existingId = existingMap.get(animal.id);

          if (existingId) {
            toUpdate.push({
              id: existingId,
              data: {
                ...mapped,
                shelter_id: shelterId,
                last_synced_at: new Date().toISOString(),
              },
            });
          } else {
            toInsert.push({
              ...mapped,
              shelter_id: shelterId,
            });
          }
        } catch {
          errors++;
        }
      }

      // Batch insert new dogs (chunks of 50)
      for (let i = 0; i < toInsert.length; i += 50) {
        const batch = toInsert.slice(i, i + 50);
        const { error: insertErr } = await supabase
          .from("dogs")
          .insert(batch);

        if (insertErr) {
          errors += batch.length;
          console.error(`Batch insert error:`, insertErr.message);
        } else {
          inserted += batch.length;
        }
      }

      // Batch update existing dogs (individual updates — can't batch different values)
      for (const item of toUpdate) {
        const { error: updateErr } = await supabase
          .from("dogs")
          .update(item.data)
          .eq("id", item.id);

        if (updateErr) {
          errors++;
        } else {
          updated++;
        }
      }

      console.log(
        `Page ${page}/${maxPages}: +${toInsert.length} inserted, ${toUpdate.length} updated, ${errors} errors`
      );

      if (response.meta && page >= response.meta.pages) break;
    } catch (err) {
      console.error(`Error fetching page ${page}:`, err);
      errors++;
      break;
    }
  }

  return {
    totalFetched,
    inserted,
    updated,
    sheltersCreated,
    errors,
    duration: Date.now() - startTime,
  };
}

/**
 * Update urgency levels for all dogs based on euthanasia_date.
 */
export async function updateUrgencyLevels(): Promise<{
  updated: number;
  errors: number;
}> {
  const supabase = createAdminClient();
  let updated = 0;
  let errors = 0;

  const { data: dogs, error } = await supabase
    .from("dogs")
    .select("id, euthanasia_date")
    .eq("is_available", true)
    .not("euthanasia_date", "is", null);

  if (error || !dogs) return { updated: 0, errors: 1 };

  const now = new Date();

  for (const dog of dogs) {
    const euthDate = new Date(dog.euthanasia_date);
    const hoursRemaining =
      (euthDate.getTime() - now.getTime()) / (1000 * 60 * 60);

    let urgencyLevel: string;
    if (hoursRemaining < 24) urgencyLevel = "critical";
    else if (hoursRemaining < 72) urgencyLevel = "high";
    else if (hoursRemaining < 168) urgencyLevel = "medium";
    else urgencyLevel = "normal";

    const { error: updateError } = await supabase
      .from("dogs")
      .update({ urgency_level: urgencyLevel })
      .eq("id", dog.id);

    if (updateError) errors++;
    else updated++;
  }

  return { updated, errors };
}
