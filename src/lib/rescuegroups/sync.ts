import { createRescueGroupsClient } from "./client";
import type { RGIncludedItem, RGOrgAttributes } from "./client";
import { mapRescueGroupsDog, extractPhotoUrls } from "./mapper";
import { createAdminClient } from "@/lib/supabase/admin";
import { validateListing } from "@/lib/utils/listing-classifier";
import { getStrayHoldDays } from "@/data/stray-hold-days";

interface SyncResult {
  totalFetched: number;
  inserted: number;
  updated: number;
  sheltersCreated: number;
  errors: number;
  rejected: number;
  duration: number;
}

interface ShelterSyncInfo {
  id: string;
  shelterType: string | null;
  stateCode: string | null;
  strayHoldDays: number | null;
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
  shelterCache: Map<string, ShelterSyncInfo>
): Promise<ShelterSyncInfo | null> {
  if (shelterCache.has(orgId)) {
    return shelterCache.get(orgId)!;
  }

  // Check DB by external_id
  const { data: existing } = await supabase
    .from("shelters")
    .select("id, shelter_type, state_code, stray_hold_days")
    .eq("external_source", "rescuegroups")
    .eq("external_id", orgId)
    .single();

  if (existing) {
    const shelterInfo: ShelterSyncInfo = {
      id: existing.id,
      shelterType: existing.shelter_type || null,
      stateCode: existing.state_code || null,
      strayHoldDays: existing.stray_hold_days ?? null,
    };
    shelterCache.set(orgId, shelterInfo);
    return shelterInfo;
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
    .select("id, shelter_type, state_code, stray_hold_days")
    .single();

  if (error) {
    // Duplicate name+state — find existing and link
    const { data: byName } = await supabase
      .from("shelters")
      .select("id, shelter_type, state_code, stray_hold_days")
      .eq("name", shelterName)
      .eq("state_code", stateCode)
      .single();

    if (byName) {
      await supabase
        .from("shelters")
        .update({ external_id: orgId, external_source: "rescuegroups" })
        .eq("id", byName.id);
      const shelterInfo: ShelterSyncInfo = {
        id: byName.id,
        shelterType: byName.shelter_type || null,
        stateCode: byName.state_code || null,
        strayHoldDays: byName.stray_hold_days ?? null,
      };
      shelterCache.set(orgId, shelterInfo);
      return shelterInfo;
    }
    return null;
  }

  const shelterInfo: ShelterSyncInfo = {
    id: newShelter.id,
    shelterType: newShelter.shelter_type || null,
    stateCode: newShelter.state_code || null,
    strayHoldDays: newShelter.stray_hold_days ?? null,
  };

  shelterCache.set(orgId, shelterInfo);
  return shelterInfo;
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
  let rejected = 0;

  const shelterCache = new Map<string, ShelterSyncInfo>();
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
      // Include date_source so we can protect verified dates from being overwritten
      const { data: existingDogs } = await supabase
        .from("dogs")
        .select("id, external_id, date_source, date_confidence, intake_date_observation_count")
        .eq("external_source", "rescuegroups")
        .in("external_id", externalIds);

      const existingMap = new Map(
        (existingDogs || []).map((d) => [d.external_id, {
          id: d.id,
          date_source: d.date_source,
          date_confidence: d.date_confidence,
          observation_count: d.intake_date_observation_count,
        }])
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

          const orgItem = included.find((i) => i.type === "orgs" && i.id === animal.relationships?.orgs?.data?.[0]?.id);
          const orgAttrs = orgItem?.attributes as unknown as RGOrgAttributes | undefined;

          // ── Stray hold adjustment: subtract hold days for municipal shelters ──
          // available_date is AFTER the hold period; true intake = available_date - hold_days
          if (
            mapped.date_source === "rescuegroups_available_date" &&
            orgAttrs
          ) {
            const shelterInfo = orgItem ? shelterCache.get(orgItem.id) : null;
            const shelterType = shelterInfo?.shelterType || mapOrgType(orgAttrs.type);
            const stateCode = (shelterInfo?.stateCode || orgAttrs.state || "").toUpperCase();

            if (shelterType === "municipal" && stateCode) {
              const holdDays = getStrayHoldDays(stateCode, shelterInfo?.strayHoldDays);
              const adjustedIntake = new Date(new Date(mapped.intake_date).getTime() - holdDays * 86400000);
              mapped.intake_date = adjustedIntake.toISOString();
              mapped.date_source += `|stray_hold_adjusted_${holdDays}d`;
            }
          }

          // ── Listing classifier gate: reject breeders/pet stores ──

          const validation = validateListing({
            name: mapped.name,
            breed: mapped.breed_primary,
            description: mapped.description,
            size: mapped.size,
            ageMonths: mapped.age_months,
            sex: mapped.gender,
            orgName: orgAttrs?.name,
            orgType: orgAttrs?.type,
          });

          if (!validation.isValid && validation.classification.shouldReject) {
            rejected++;
            continue;
          }

          const orgId = animal.relationships?.orgs?.data?.[0]?.id;
          const shelterInfo = orgId ? shelterCache.get(orgId) : null;
          const shelterId = shelterInfo?.id || null;

          if (!shelterId) {
            errors++;
            continue;
          }

          // Build source_links for data provenance
          const sourceLinks = [
            {
              url: `https://www.rescuegroups.org/animals/${animal.id}`,
              source: "RescueGroups.org",
              checked_at: new Date().toISOString(),
              status_code: null,
              description: "Primary listing on RescueGroups.org database",
            },
          ];
          if (orgAttrs?.url) {
            sourceLinks.push({
              url: orgAttrs.url.substring(0, 500),
              source: "Shelter Website",
              checked_at: new Date().toISOString(),
              status_code: null,
              description: `Official website of ${(orgAttrs.name || "shelter").substring(0, 100)}`,
            });
          }
          if (orgId) {
            sourceLinks.push({
              url: `https://www.rescuegroups.org/organizations/${orgId}`,
              source: "RescueGroups.org Organization",
              checked_at: new Date().toISOString(),
              status_code: null,
              description: "Organization profile on RescueGroups.org",
            });
          }

          const existing = existingMap.get(animal.id);

          if (existing) {
            // Never downgrade date quality on sync.
            const CONFIDENCE_RANK: Record<string, number> = {
              verified: 5,
              high: 4,
              medium: 3,
              low: 2,
              unknown: 1,
            };
            const existingRank = CONFIDENCE_RANK[existing.date_confidence || "unknown"] || 0;
            const newRank = CONFIDENCE_RANK[mapped.date_confidence] || 0;

            // eslint-disable-next-line @typescript-eslint/no-explicit-any
            const updateData: Record<string, any> = {
              ...mapped,
              shelter_id: shelterId,
              last_synced_at: new Date().toISOString(),
              source_links: sourceLinks,
            };

            // If existing date is equal or better quality, don't overwrite
            if (existingRank >= newRank) {
              delete updateData.intake_date;
              delete updateData.date_confidence;
              delete updateData.date_source;
              // Crawl consistency: same date confirmed again → increment observation count
              updateData.intake_date_observation_count = (existing.observation_count || 1) + 1;
            } else {
              // Date quality upgraded → reset observation count
              updateData.intake_date_observation_count = 1;
            }

            // Auto-set ranking_eligible when date is verified
            const effectiveConfidence = existingRank >= newRank
              ? existing.date_confidence
              : mapped.date_confidence;
            if (effectiveConfidence === "verified") {
              updateData.ranking_eligible = true;
            }

            toUpdate.push({
              id: existing.id,
              data: updateData,
            });
          } else {
            toInsert.push({
              ...mapped,
              shelter_id: shelterId,
              source_links: sourceLinks,
              original_intake_date: mapped.intake_date,
              ranking_eligible: mapped.date_confidence === "verified",
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
    rejected,
    duration: Date.now() - startTime,
  };
}

/**
 * Update urgency levels for all dogs based on euthanasia_date.
 *
 * HARD RULE: Only dogs with a real euthanasia_date can be high/critical.
 * Any dog found at high/critical WITHOUT a date gets reset to normal.
 * This prevents false positives from ever showing on the /urgent page.
 */
export async function updateUrgencyLevels(): Promise<{
  updated: number;
  errors: number;
  guardrailResets: number;
}> {
  const supabase = createAdminClient();
  let updated = 0;
  let errors = 0;
  let guardrailResets = 0;

  // Step 1: Update urgency for dogs WITH euthanasia dates
  const { data: dogs, error } = await supabase
    .from("dogs")
    .select("id, euthanasia_date, urgency_level")
    .eq("is_available", true)
    .not("euthanasia_date", "is", null);

  if (error || !dogs) return { updated: 0, errors: 1, guardrailResets: 0 };

  const now = new Date();

  for (const dog of dogs) {
    const euthDate = new Date(dog.euthanasia_date);
    const hoursRemaining =
      (euthDate.getTime() - now.getTime()) / (1000 * 60 * 60);

    let urgencyLevel: string;
    let shouldDeactivate = false;
    if (hoursRemaining <= -48) {
      // Expired >48h ago — deactivate (outcome unknown, likely euthanized or adopted)
      urgencyLevel = "normal";
      shouldDeactivate = true;
    } else if (hoursRemaining <= 0) {
      // Recently expired (0-48h) — remove from urgent pages, Freshness Agent will verify
      urgencyLevel = "normal";
    } else if (hoursRemaining < 24) {
      urgencyLevel = "critical";
    } else if (hoursRemaining < 72) {
      urgencyLevel = "high";
    } else if (hoursRemaining < 168) {
      urgencyLevel = "medium";
    } else {
      urgencyLevel = "normal";
    }

    if (shouldDeactivate) {
      const { error: deactErr } = await supabase
        .from("dogs")
        .update({
          urgency_level: "normal",
          is_available: false,
          status: "outcome_unknown",
        })
        .eq("id", dog.id);
      if (deactErr) errors++;
      else updated++;
    } else if (dog.urgency_level !== urgencyLevel) {
      const { error: updateError } = await supabase
        .from("dogs")
        .update({ urgency_level: urgencyLevel })
        .eq("id", dog.id);

      if (updateError) errors++;
      else updated++;
    }
  }

  // Step 2: GUARDRAIL — Reset any high/critical dogs that have NO euthanasia_date
  // This catches false positives from description parsing or stale data
  const { data: falsePosHigh } = await supabase
    .from("dogs")
    .select("id")
    .in("urgency_level", ["high", "critical"])
    .is("euthanasia_date", null);

  if (falsePosHigh && falsePosHigh.length > 0) {
    const ids = falsePosHigh.map((d) => d.id);
    const { error: resetError } = await supabase
      .from("dogs")
      .update({
        urgency_level: "normal",
        is_on_euthanasia_list: false,
        euthanasia_list_added_at: null,
      })
      .in("id", ids);

    if (!resetError) {
      guardrailResets = ids.length;
    }
  }

  return { updated, errors, guardrailResets };
}
