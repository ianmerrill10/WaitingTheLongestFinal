/**
 * Cross-platform deduplication engine.
 * Detects and merges duplicate dogs that appear from both RescueGroups and the scraper.
 * Matching: same shelter_id + exact name match + overlapping breed.
 * Merge: keeps RescueGroups record as canonical, copies better fields from scraper.
 */
import { createAdminClient } from "@/lib/supabase/admin";

interface DogRecord {
  id: string;
  name: string;
  shelter_id: string;
  breed_primary: string;
  external_id: string | null;
  external_source: string | null;
  description: string | null;
  photo_urls: string[] | null;
  primary_photo_url: string | null;
  intake_date: string | null;
  date_confidence: string | null;
  euthanasia_date: string | null;
  weight_lbs: number | null;
  color_primary: string | null;
  good_with_kids: boolean | null;
  good_with_dogs: boolean | null;
  good_with_cats: boolean | null;
  house_trained: boolean | null;
  dedup_merged_from: string[];
}

const CONFIDENCE_RANK: Record<string, number> = {
  verified: 5, high: 4, medium: 3, low: 2, unknown: 1,
};

/**
 * Check if a scraped dog already exists in the DB from a different source.
 * Returns the existing record if found, null otherwise.
 */
export async function findDuplicate(
  shelterId: string,
  name: string,
  breedPrimary: string,
  externalSource: string
): Promise<DogRecord | null> {
  const supabase = createAdminClient();

  // Exact name + shelter + different source
  const { data } = await supabase
    .from("dogs")
    .select("id, name, shelter_id, breed_primary, external_id, external_source, description, photo_urls, primary_photo_url, intake_date, date_confidence, euthanasia_date, weight_lbs, color_primary, good_with_kids, good_with_dogs, good_with_cats, house_trained, dedup_merged_from")
    .eq("shelter_id", shelterId)
    .eq("is_available", true)
    .ilike("name", name.trim())
    .neq("external_source", externalSource)
    .limit(5);

  if (!data || data.length === 0) return null;

  // Find best match by breed overlap
  const normalizedBreed = breedPrimary.toLowerCase().trim();
  for (const dog of data) {
    const existingBreed = (dog.breed_primary || "").toLowerCase().trim();
    // Match if breeds overlap (either contains the other or share first word)
    if (
      existingBreed === normalizedBreed ||
      existingBreed.includes(normalizedBreed) ||
      normalizedBreed.includes(existingBreed) ||
      existingBreed.split(/[\s/]+/)[0] === normalizedBreed.split(/[\s/]+/)[0]
    ) {
      return dog as DogRecord;
    }
  }

  return null;
}

/**
 * Merge a scraped dog into an existing record, keeping the best data from each.
 * The existing record (usually RescueGroups) is the canonical one.
 */
export async function mergeDuplicate(
  existingId: string,
  existing: DogRecord,
  scraperData: Record<string, unknown>,
  scraperExternalId: string
): Promise<boolean> {
  const supabase = createAdminClient();

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const updates: Record<string, any> = {};

  // Copy fields from scraper where existing is missing
  if (!existing.description && scraperData.description) {
    updates.description = scraperData.description;
  }
  if ((!existing.photo_urls || existing.photo_urls.length === 0) && scraperData.photo_urls) {
    updates.photo_urls = scraperData.photo_urls;
    updates.primary_photo_url = scraperData.primary_photo_url;
  }
  if (!existing.weight_lbs && scraperData.weight_lbs) {
    updates.weight_lbs = scraperData.weight_lbs;
  }
  if (!existing.color_primary && scraperData.color_primary) {
    updates.color_primary = scraperData.color_primary;
  }
  if (existing.good_with_kids === null && scraperData.good_with_kids !== null) {
    updates.good_with_kids = scraperData.good_with_kids;
  }
  if (existing.good_with_dogs === null && scraperData.good_with_dogs !== null) {
    updates.good_with_dogs = scraperData.good_with_dogs;
  }
  if (existing.good_with_cats === null && scraperData.good_with_cats !== null) {
    updates.good_with_cats = scraperData.good_with_cats;
  }
  if (existing.house_trained === null && scraperData.house_trained !== null) {
    updates.house_trained = scraperData.house_trained;
  }

  // Use better intake_date if scraper has higher confidence
  const existingRank = CONFIDENCE_RANK[existing.date_confidence || "unknown"] || 0;
  const scraperRank = CONFIDENCE_RANK[(scraperData.date_confidence as string) || "unknown"] || 0;
  if (scraperRank > existingRank) {
    updates.intake_date = scraperData.intake_date;
    updates.date_confidence = scraperData.date_confidence;
    updates.date_source = scraperData.date_source;
  }

  // Use euthanasia_date from scraper if existing doesn't have one
  if (!existing.euthanasia_date && scraperData.euthanasia_date) {
    updates.euthanasia_date = scraperData.euthanasia_date;
    updates.urgency_level = scraperData.urgency_level;
    updates.is_on_euthanasia_list = scraperData.is_on_euthanasia_list;
  }

  // Track the merge
  const mergedFrom = existing.dedup_merged_from || [];
  if (!mergedFrom.includes(scraperExternalId)) {
    mergedFrom.push(scraperExternalId);
    updates.dedup_merged_from = mergedFrom;
  }

  if (Object.keys(updates).length <= 1) return false; // Only dedup_merged_from, nothing useful

  updates.updated_at = new Date().toISOString();

  const { error } = await supabase
    .from("dogs")
    .update(updates)
    .eq("id", existingId);

  return !error;
}

/**
 * Run a batch dedup pass across all dogs in a shelter.
 * Finds scraper-sourced dogs that duplicate RG-sourced dogs and merges them.
 */
export async function runBatchDedup(shelterId?: string): Promise<{
  checked: number;
  merged: number;
  removed: number;
}> {
  const supabase = createAdminClient();
  let checked = 0;
  let merged = 0;
  let removed = 0;

  // Find scraper-sourced dogs
  let query = supabase
    .from("dogs")
    .select("id, name, shelter_id, breed_primary, external_id, external_source, description, photo_urls, primary_photo_url, intake_date, date_confidence, euthanasia_date, weight_lbs, color_primary, good_with_kids, good_with_dogs, good_with_cats, house_trained, dedup_merged_from")
    .eq("is_available", true)
    .like("external_source", "scraper_%")
    .limit(1000);

  if (shelterId) {
    query = query.eq("shelter_id", shelterId);
  }

  const { data: scraperDogs } = await query;
  if (!scraperDogs || scraperDogs.length === 0) return { checked, merged, removed };

  for (const scraperDog of scraperDogs) {
    checked++;

    // Look for RG-sourced duplicate
    const { data: rgDogs } = await supabase
      .from("dogs")
      .select("id, name, shelter_id, breed_primary, external_id, external_source, description, photo_urls, primary_photo_url, intake_date, date_confidence, euthanasia_date, weight_lbs, color_primary, good_with_kids, good_with_dogs, good_with_cats, house_trained, dedup_merged_from")
      .eq("shelter_id", scraperDog.shelter_id)
      .eq("is_available", true)
      .eq("external_source", "rescuegroups")
      .ilike("name", scraperDog.name.trim())
      .limit(3);

    if (!rgDogs || rgDogs.length === 0) continue;

    // Find breed match
    const scraperBreed = (scraperDog.breed_primary || "").toLowerCase();
    const match = rgDogs.find((rg) => {
      const rgBreed = (rg.breed_primary || "").toLowerCase();
      return (
        rgBreed === scraperBreed ||
        rgBreed.includes(scraperBreed) ||
        scraperBreed.includes(rgBreed) ||
        rgBreed.split(/[\s/]+/)[0] === scraperBreed.split(/[\s/]+/)[0]
      );
    });

    if (match) {
      // Merge scraper data into RG record
      const didMerge = await mergeDuplicate(
        match.id,
        match as DogRecord,
        scraperDog,
        scraperDog.external_id
      );

      if (didMerge) {
        merged++;
        // Deactivate the scraper duplicate
        await supabase
          .from("dogs")
          .update({
            is_available: false,
            status: "duplicate_merged",
            updated_at: new Date().toISOString(),
          })
          .eq("id", scraperDog.id);
        removed++;
      }
    }
  }

  return { checked, merged, removed };
}
