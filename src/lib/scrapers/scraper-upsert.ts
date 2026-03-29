/**
 * Scraper upsert engine — inserts new dogs, updates existing, deactivates adopted.
 * Follows patterns from src/lib/rescuegroups/sync.ts.
 */
import { createAdminClient } from "@/lib/supabase/admin";
import { validateListing } from "@/lib/utils/listing-classifier";
import { mapScrapedDog } from "./dog-mapper";
import { findDuplicate, mergeDuplicate } from "./dedup-engine";
import { getStrayHoldDays } from "@/data/stray-hold-days";
import type { ScrapedDog, UpsertResult } from "./types";

const CONFIDENCE_RANK: Record<string, number> = {
  verified: 5,
  high: 4,
  medium: 3,
  low: 2,
  unknown: 1,
};

/**
 * Upsert scraped dogs for a shelter.
 * - Insert new dogs
 * - Update existing dogs (never downgrade date quality)
 * - Deactivate dogs that are no longer on the shelter's website (adopted!)
 */
export async function upsertScrapedDogs(
  shelterId: string,
  dogs: ScrapedDog[],
  platform: string
): Promise<UpsertResult> {
  const supabase = createAdminClient();
  const externalSource = `scraper_${platform}`;
  let inserted = 0;
  let updated = 0;
  let deactivated = 0;
  let errors = 0;

  // Fetch shelter info for stray hold adjustment
  const { data: shelterInfo } = await supabase
    .from("shelters")
    .select("state_code, shelter_type, stray_hold_days")
    .eq("id", shelterId)
    .single();

  // Step 1: Get all existing dogs from this shelter with this external source
  const { data: existingDogs } = await supabase
    .from("dogs")
    .select("id, external_id, date_source, date_confidence, is_available, intake_date_observation_count")
    .eq("shelter_id", shelterId)
    .eq("external_source", externalSource);

  const existingMap = new Map(
    (existingDogs || []).map((d) => [
      d.external_id,
      {
        id: d.id,
        date_source: d.date_source,
        date_confidence: d.date_confidence,
        is_available: d.is_available,
        observation_count: d.intake_date_observation_count,
      },
    ])
  );

  // Track which dogs we saw in this scrape
  const seenExternalIds = new Set<string>();

  // Step 2: Process each scraped dog
  const toInsert: Record<string, unknown>[] = [];
  const toUpdate: { id: string; data: Record<string, unknown> }[] = [];

  for (const dog of dogs) {
    try {
      // Validate listing (reject breeders, pet stores, non-dog listings)
      const validation = validateListing({
        name: dog.name,
        breed: dog.breed_primary,
        description: dog.description,
        size: dog.size,
        ageMonths: dog.age_months,
        sex: dog.gender,
      });

      if (!validation.isValid && validation.classification.shouldReject) {
        continue;
      }

      const mapped = mapScrapedDog(dog, shelterId, platform);

      // Stray hold adjustment for municipal shelters
      // available_date is AFTER hold; true intake = available_date - hold_days
      if (
        shelterInfo?.shelter_type === "municipal" &&
        typeof mapped.date_source === "string" &&
        mapped.date_source.includes("listing_date")
      ) {
        const holdDays = getStrayHoldDays(
          shelterInfo.state_code || "",
          shelterInfo.stray_hold_days
        );
        const adjustedIntake = new Date(
          new Date(mapped.intake_date as string).getTime() - holdDays * 86400000
        );
        mapped.intake_date = adjustedIntake.toISOString();
        mapped.date_source = `${mapped.date_source}|stray_hold_adjusted_${holdDays}d`;
      }

      seenExternalIds.add(dog.external_id);

      // Build source_links for data provenance
      const sourceLinks: { url: string; source: string; checked_at: string; status_code: number | null; description: string }[] = [];
      if (dog.external_url) {
        sourceLinks.push({
          url: dog.external_url.substring(0, 500),
          source: `Scraped from ${platform}`,
          checked_at: new Date().toISOString(),
          status_code: 200,
          description: `Dog listing page scraped via ${platform} adapter`,
        });
      }

      const existing = existingMap.get(dog.external_id);

      if (existing) {
        // Update existing dog — never downgrade date confidence
        const existingRank =
          CONFIDENCE_RANK[existing.date_confidence || "unknown"] || 0;
        const newRank =
          CONFIDENCE_RANK[mapped.date_confidence as string] || 0;

        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        const updateData: Record<string, any> = {
          ...mapped,
          state_code: shelterInfo?.state_code || null,
          last_synced_at: new Date().toISOString(),
          is_available: true,
          verification_status: "verified",
          last_verified_at: new Date().toISOString(),
          source_links: sourceLinks.length > 0 ? sourceLinks : undefined,
        };

        // Protect better-quality dates
        if (existingRank >= newRank) {
          delete updateData.intake_date;
          delete updateData.date_confidence;
          delete updateData.date_source;
          // Crawl consistency: same date confirmed → increment observation count
          updateData.intake_date_observation_count = (existing.observation_count || 1) + 1;
        } else {
          // Date quality upgraded → reset observation count
          updateData.intake_date_observation_count = 1;
        }

        // Auto-set ranking_eligible for verified dates
        const effectiveConfidence = existingRank >= newRank
          ? existing.date_confidence
          : (mapped.date_confidence as string);
        if (effectiveConfidence === "verified") {
          updateData.ranking_eligible = true;
        }

        toUpdate.push({ id: existing.id, data: updateData });
      } else {
        // Check for cross-platform duplicate before inserting
        try {
          const dup = await findDuplicate(
            shelterId,
            dog.name,
            dog.breed_primary,
            externalSource
          );
          if (dup) {
            // Merge into existing record instead of creating duplicate
            await mergeDuplicate(dup.id, dup, mapped, dog.external_id);
            updated++; // Count as update since we enriched existing record
            seenExternalIds.add(dog.external_id); // Mark as seen to prevent deactivation
            continue;
          }
        } catch {
          // Dedup check failed — proceed with insert
        }
        toInsert.push({
          ...mapped,
          state_code: shelterInfo?.state_code || null,
          source_links: sourceLinks.length > 0 ? sourceLinks : [],
          original_intake_date: mapped.intake_date as string,
          ranking_eligible: mapped.date_confidence === "verified",
        });
      }
    } catch {
      errors++;
    }
  }

  // Step 3: Batch insert new dogs (chunks of 50)
  for (let i = 0; i < toInsert.length; i += 50) {
    const batch = toInsert.slice(i, i + 50);
    const { error: insertErr } = await supabase.from("dogs").insert(batch);
    if (insertErr) {
      errors += batch.length;
    } else {
      inserted += batch.length;
    }
  }

  // Step 4: Update existing dogs (batched in parallel chunks of 10)
  for (let i = 0; i < toUpdate.length; i += 10) {
    const batch = toUpdate.slice(i, i + 10);
    const results = await Promise.all(
      batch.map((item) =>
        supabase.from("dogs").update(item.data).eq("id", item.id)
      )
    );
    for (const result of results) {
      if (result.error) errors++;
      else updated++;
    }
  }

  // Step 5: Deactivate dogs that are NO LONGER on the shelter's website
  // This is the GROUND TRUTH: if the shelter removed the listing, the dog is gone
  // Only deactivate dogs from THIS external source (don't touch RG-sourced dogs)
  if (dogs.length > 0) {
    // Only do adoption detection if we actually scraped some dogs
    // (prevents marking all dogs as adopted if the scrape failed/returned empty)
    const toDeactivate: string[] = [];
    for (const [extId, existing] of Array.from(existingMap.entries())) {
      if (existing.is_available && !seenExternalIds.has(extId)) {
        toDeactivate.push(existing.id);
      }
    }

    if (toDeactivate.length > 0) {
      // Don't deactivate more than 50% of dogs at once (safety guard)
      const maxDeactivate = Math.max(
        Math.floor(existingMap.size * 0.5),
        5
      );
      const idsToDeactivate = toDeactivate.slice(0, maxDeactivate);

      for (let i = 0; i < idsToDeactivate.length; i += 50) {
        const batch = idsToDeactivate.slice(i, i + 50);
        const { error: deactivateErr } = await supabase
          .from("dogs")
          .update({
            is_available: false,
            status: "adopted",
            verification_status: "not_found",
            updated_at: new Date().toISOString(),
          })
          .in("id", batch);

        if (deactivateErr) errors++;
        else deactivated += batch.length;
      }
    }
  }

  return { inserted, updated, deactivated, errors };
}
