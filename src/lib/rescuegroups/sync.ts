import { createRescueGroupsClient } from "./client";
import { mapRescueGroupsDog } from "./mapper";
import { createAdminClient } from "@/lib/supabase/admin";

interface SyncResult {
  totalFetched: number;
  inserted: number;
  updated: number;
  errors: number;
  duration: number;
}

/**
 * Sync dogs from RescueGroups API to Supabase.
 * Fetches available dogs page by page, maps to our schema,
 * and upserts into the dogs table using external_id for dedup.
 */
export async function syncDogsFromRescueGroups(
  maxPages: number = 50
): Promise<SyncResult> {
  const startTime = Date.now();
  const client = createRescueGroupsClient();
  const supabase = createAdminClient();

  let totalFetched = 0;
  let inserted = 0;
  let updated = 0;
  let errors = 0;

  for (let page = 1; page <= maxPages; page++) {
    try {
      const response = await client.fetchAnimals(page, 250);

      if (!response.data || response.data.length === 0) {
        break; // No more data
      }

      totalFetched += response.data.length;

      // Map and upsert each dog
      for (const animal of response.data) {
        try {
          const mapped = mapRescueGroupsDog(animal.id, animal.attributes);

          // Get shelter_id from org relationship
          const orgId =
            animal.relationships?.orgs?.data?.[0]?.id || null;

          // Check if dog already exists by external_id
          const { data: existing } = await supabase
            .from("dogs")
            .select("id")
            .eq("external_source", "rescuegroups")
            .eq("external_id", animal.id)
            .single();

          if (existing) {
            // Update existing dog
            const { error } = await supabase
              .from("dogs")
              .update({
                ...mapped,
                shelter_id: existing.id, // Keep existing shelter_id
                last_synced_at: new Date().toISOString(),
              })
              .eq("id", existing.id);

            if (error) {
              errors++;
              console.error(`Error updating dog ${animal.id}:`, error.message);
            } else {
              updated++;
            }
          } else {
            // Find or create shelter for this org
            let shelterId: string | null = null;

            if (orgId) {
              const { data: shelter } = await supabase
                .from("shelters")
                .select("id")
                .eq("external_source", "rescuegroups")
                .eq("external_id", orgId)
                .single();

              shelterId = shelter?.id || null;
            }

            // If no shelter found, skip (we need a valid shelter_id)
            if (!shelterId) {
              errors++;
              continue;
            }

            // Insert new dog
            const { error } = await supabase.from("dogs").insert({
              ...mapped,
              shelter_id: shelterId,
            });

            if (error) {
              errors++;
              console.error(`Error inserting dog ${animal.id}:`, error.message);
            } else {
              inserted++;
            }
          }
        } catch (err) {
          errors++;
          console.error(`Error processing animal ${animal.id}:`, err);
        }
      }

      // Check if we've reached the last page
      if (
        response.meta &&
        page >= response.meta.pages
      ) {
        break;
      }
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

  // Fetch dogs with euthanasia dates
  const { data: dogs, error } = await supabase
    .from("dogs")
    .select("id, euthanasia_date")
    .eq("is_available", true)
    .not("euthanasia_date", "is", null);

  if (error || !dogs) {
    return { updated: 0, errors: 1 };
  }

  const now = new Date();

  for (const dog of dogs) {
    const euthDate = new Date(dog.euthanasia_date);
    const hoursRemaining =
      (euthDate.getTime() - now.getTime()) / (1000 * 60 * 60);

    let urgencyLevel: string;
    if (hoursRemaining <= 0) {
      urgencyLevel = "critical"; // Expired but not yet updated
    } else if (hoursRemaining < 24) {
      urgencyLevel = "critical";
    } else if (hoursRemaining < 72) {
      urgencyLevel = "high";
    } else if (hoursRemaining < 168) {
      urgencyLevel = "medium";
    } else {
      urgencyLevel = "normal";
    }

    const { error: updateError } = await supabase
      .from("dogs")
      .update({ urgency_level: urgencyLevel })
      .eq("id", dog.id);

    if (updateError) {
      errors++;
    } else {
      updated++;
    }
  }

  return { updated, errors };
}
