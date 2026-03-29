/**
 * Government Open Data Sync — ingests dogs from Socrata portals.
 * These records provide GOLD STANDARD intake dates (official government records).
 *
 * Flow:
 * 1. For each active gov data source, fetch current dogs
 * 2. Find or create the shelter in our DB
 * 3. Cross-reference with existing dogs by name+breed
 * 4. If dog exists: upgrade intake_date to government-verified date
 * 5. If dog is new: insert with verified confidence
 */

import { createAdminClient } from "@/lib/supabase/admin";
import { getActiveGovSources } from "@/data/gov-open-data-sources";
import { fetchSocrataAnimals, type GovDogRecord } from "./socrata";

interface GovSyncResult {
  source: string;
  fetched: number;
  dateUpgrades: number;
  newInserts: number;
  shelterMatched: boolean;
  errors: number;
}

/**
 * Sync dogs from all active government open data sources.
 * Primary purpose: upgrade intake_date confidence for dogs already in our DB.
 */
export async function syncGovOpenData(
  stateFilter?: string
): Promise<GovSyncResult[]> {
  const sources = getActiveGovSources(stateFilter);
  const results: GovSyncResult[] = [];

  for (const source of sources) {
    try {
      const result = await syncSingleGovSource(source.name);
      results.push(result);
    } catch (err) {
      console.error(`[GovSync] Error syncing ${source.name}:`, err);
      results.push({
        source: source.name,
        fetched: 0,
        dateUpgrades: 0,
        newInserts: 0,
        shelterMatched: false,
        errors: 1,
      });
    }
  }

  return results;
}

async function syncSingleGovSource(sourceName: string): Promise<GovSyncResult> {
  const sources = getActiveGovSources();
  const source = sources.find((s) => s.name === sourceName);
  if (!source) {
    return { source: sourceName, fetched: 0, dateUpgrades: 0, newInserts: 0, shelterMatched: false, errors: 1 };
  }

  const supabase = createAdminClient();
  let dateUpgrades = 0;
  let newInserts = 0;
  let errors = 0;

  // Fetch animals from gov portal
  const govDogs = await fetchSocrataAnimals(source);
  if (govDogs.length === 0) {
    return { source: source.name, fetched: 0, dateUpgrades: 0, newInserts: 0, shelterMatched: false, errors: 0 };
  }

  // Find matching shelter in our DB by name + city + state
  const { data: shelterMatch } = await supabase
    .from("shelters")
    .select("id, name, state_code")
    .eq("state_code", source.state)
    .ilike("city", source.city)
    .limit(10);

  // Try exact name match first, then fuzzy
  let shelterId: string | null = null;
  if (shelterMatch && shelterMatch.length > 0) {
    const exact = shelterMatch.find(
      (s) => s.name.toLowerCase().includes(source.name.toLowerCase().split(" ")[0])
    );
    shelterId = exact?.id || shelterMatch[0].id;
  }

  if (!shelterId) {
    // Create the shelter
    const { data: newShelter } = await supabase
      .from("shelters")
      .insert({
        name: source.name,
        city: source.city,
        state_code: source.state,
        shelter_type: "municipal",
        external_source: "gov_open_data",
        external_id: `${source.domain}/${source.datasetId}`,
        is_verified: true,
      })
      .select("id")
      .single();

    if (!newShelter) {
      return { source: source.name, fetched: govDogs.length, dateUpgrades: 0, newInserts: 0, shelterMatched: false, errors: 1 };
    }
    shelterId = newShelter.id;
  }

  // Get all existing dogs at this shelter for cross-reference
  const { data: existingDogs } = await supabase
    .from("dogs")
    .select("id, name, breed_primary, intake_date, date_confidence, date_source, external_id, intake_date_observation_count")
    .eq("shelter_id", shelterId)
    .eq("is_available", true)
    .limit(5000);

  // Index by lowercase name for fast lookup
  const existingByName = new Map<string, typeof existingDogs>();
  for (const dog of existingDogs || []) {
    const key = (dog.name || "").toLowerCase().trim();
    const list = existingByName.get(key) || [];
    list.push(dog);
    existingByName.set(key, list);
  }

  // Process each gov dog record
  for (const govDog of govDogs) {
    try {
      const nameKey = govDog.name.toLowerCase().trim();
      const candidates = existingByName.get(nameKey);

      if (candidates && candidates.length > 0) {
        // Dog exists — try to upgrade the intake date
        const match = findBestBreedMatch(candidates, govDog.breed);
        if (match) {
          // Only upgrade if gov date is EARLIER or existing confidence is below verified
          const govDate = new Date(govDog.intakeDate);
          const existingDate = match.intake_date ? new Date(match.intake_date) : null;
          const isUpgrade =
            match.date_confidence !== "verified" ||
            (existingDate && govDate < existingDate);

          if (isUpgrade) {
            const { error } = await supabase
              .from("dogs")
              .update({
                intake_date: govDate.toISOString(),
                date_confidence: "verified",
                date_source: `gov_opendata:${source.name}`,
                original_intake_date: match.intake_date || govDate.toISOString(),
                ranking_eligible: true,
                intake_date_observation_count: 1, // Reset — new verified date
              })
              .eq("id", match.id);

            if (!error) dateUpgrades++;
            else errors++;
          } else {
            // Same date confirmed by gov source → increment observation count
            await supabase
              .from("dogs")
              .update({
                intake_date_observation_count: (match.intake_date_observation_count || 1) + 1,
                ranking_eligible: true,
              })
              .eq("id", match.id);
          }
          continue;
        }
      }

      // Dog not found in our DB — insert as new
      // Only insert if we have enough info
      if (govDog.name && govDog.intakeDate && govDog.breed) {
        const breed = parseGovBreed(govDog.breed);
        const { error } = await supabase.from("dogs").insert({
          name: govDog.name.substring(0, 200),
          shelter_id: shelterId,
          breed_primary: breed.primary.substring(0, 100),
          breed_secondary: breed.secondary?.substring(0, 100) || null,
          breed_mixed: breed.mixed,
          size: "medium",
          age_category: "adult",
          gender: govDog.sex === "female" ? "female" : "male",
          color_primary: govDog.color?.substring(0, 50) || null,
          status: "available",
          is_available: true,
          intake_date: new Date(govDog.intakeDate).toISOString(),
          date_confidence: "verified",
          date_source: `gov_opendata:${source.name}`,
          intake_type: govDog.intakeType?.toLowerCase() || null,
          external_id: govDog.sourceId,
          external_source: `gov_opendata`,
          source_extraction_method: `socrata_api:${source.datasetId}`,
          verification_status: "verified",
          last_verified_at: new Date().toISOString(),
          original_intake_date: new Date(govDog.intakeDate).toISOString(),
          ranking_eligible: true,
          source_links: [
            {
              url: `https://${source.domain}/resource/${source.datasetId}`,
              source: "Government Open Data Portal",
              checked_at: new Date().toISOString(),
              status_code: 200,
              description: `Official records from ${source.name}`,
            },
          ],
        });

        if (!error) newInserts++;
        else errors++;
      }
    } catch {
      errors++;
    }
  }

  return {
    source: source.name,
    fetched: govDogs.length,
    dateUpgrades,
    newInserts,
    shelterMatched: true,
    errors,
  };
}

// eslint-disable-next-line @typescript-eslint/no-explicit-any
function findBestBreedMatch(candidates: any[], govBreed: string | null): any {
  if (!govBreed) return candidates[0];

  const govBreedLower = govBreed.toLowerCase();
  for (const dog of candidates) {
    const dbBreed = (dog.breed_primary || "").toLowerCase();
    if (
      dbBreed === govBreedLower ||
      govBreedLower.includes(dbBreed) ||
      dbBreed.includes(govBreedLower.split("/")[0].trim())
    ) {
      return dog;
    }
  }
  return candidates[0]; // If no breed match, still match by name
}

function parseGovBreed(breedStr: string): {
  primary: string;
  secondary: string | null;
  mixed: boolean;
} {
  const parts = breedStr.split(/[\/\-]/);
  const primary = parts[0]?.trim() || "Mixed Breed";
  const secondary = parts[1]?.trim() || null;
  const mixed =
    breedStr.toLowerCase().includes("mix") ||
    breedStr.toLowerCase().includes("cross") ||
    parts.length > 1;

  return { primary, secondary, mixed };
}
