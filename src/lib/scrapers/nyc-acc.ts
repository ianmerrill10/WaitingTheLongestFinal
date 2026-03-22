/**
 * NYC ACC (Animal Care Centers of NYC) Scraper
 *
 * Scrapes the Priority Placement page at https://www.nycacc.org/priority-placement/
 * This page lists 150+ dogs at highest risk — long stays, behavioral concerns, etc.
 *
 * Also fetches individual animal details from:
 * https://nycaccpets.shelterbuddy.com/animal/animalDetails.asp?task=view&searchType=4&animalid={ID}
 *
 * Data extracted:
 * - Animal name, ID, age, weight, days in care, reason code
 * - Breed, sex, color, description, photos, behavior level (from detail page)
 *
 * Reason codes: LOS=Length of Stay, BH=Behavioral Health, DA=Dog Aggressive,
 * FAS=Fear/Anxiety/Stress, HTS=Hard to Shelter, PIC=Previously In Care,
 * LP=Long-term Placement, DB=Difficult Behavior, OLR=Owner-Linked Return,
 * LEVEL 4=Level 4 behavioral assessment
 */

import * as cheerio from "cheerio";
import { createAdminClient } from "@/lib/supabase/admin";

const PRIORITY_PLACEMENT_URL = "https://www.nycacc.org/priority-placement/";
const DETAIL_BASE_URL = "https://nycaccpets.shelterbuddy.com/animal/animalDetails.asp";
const ACC_APP_BASE = "https://nycacc.app/#/browse/";

// NYC ACC shelter locations
const NYC_ACC_SHELTERS = {
  manhattan: { name: "Manhattan Animal Care Center", city: "New York", state: "NY" },
  brooklyn: { name: "Brooklyn Animal Care Center", city: "Brooklyn", state: "NY" },
  staten_island: { name: "Staten Island Animal Care Center", city: "Staten Island", state: "NY" },
};

export interface NycAccDog {
  animalId: string;
  name: string;
  age: string | null;
  ageMonths: number | null;
  weight: string | null;
  weightLbs: number | null;
  daysInCare: number | null;
  reasonCode: string | null;
  profileUrl: string;
  // From detail page (optional — fetched separately)
  breed: string | null;
  sex: string | null;
  color: string | null;
  description: string | null;
  photoUrls: string[];
  location: string | null;
  behaviorLevel: string | null;
  spayedNeutered: boolean | null;
}

export interface NycAccScrapeResult {
  dogs: NycAccDog[];
  scrapedAt: string;
  source: string;
  errors: string[];
}

/**
 * Parse age string like "3 Years 3 Months 1 Week" into approximate months
 */
function parseAgeToMonths(ageStr: string): number | null {
  if (!ageStr) return null;

  let months = 0;
  const yearMatch = ageStr.match(/(\d+)\s*year/i);
  const monthMatch = ageStr.match(/(\d+)\s*month/i);
  const weekMatch = ageStr.match(/(\d+)\s*week/i);

  if (yearMatch) months += parseInt(yearMatch[1]) * 12;
  if (monthMatch) months += parseInt(monthMatch[1]);
  if (weekMatch) months += Math.round(parseInt(weekMatch[1]) / 4.33);

  return months > 0 ? months : null;
}

/**
 * Parse weight string like "60lbs" or "61.6lbs" to number
 */
function parseWeightLbs(weightStr: string): number | null {
  if (!weightStr) return null;
  const match = weightStr.match(/([\d.]+)\s*(?:lbs?|pounds?)/i);
  return match ? parseFloat(match[1]) : null;
}

/**
 * Scrape the Priority Placement page for the list of at-risk dogs.
 * This is the primary entry point — no auth required.
 */
export async function scrapePriorityPlacement(): Promise<NycAccScrapeResult> {
  const errors: string[] = [];
  const dogs: NycAccDog[] = [];

  try {
    // Fetch the priority placement page
    const response = await fetch(PRIORITY_PLACEMENT_URL, {
      headers: {
        "User-Agent": "WaitingTheLongest.com/1.0 (Dog Rescue Aggregator; +https://waitingthelongest.com)",
        Accept: "text/html",
      },
    });

    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }

    const html = await response.text();
    const $ = cheerio.load(html);

    // The Ninja Tables plugin renders an HTML table
    // Look for the table with animal data
    $("table tbody tr").each((_i, row) => {
      try {
        const cells = $(row).find("td");
        if (cells.length < 3) return;

        // Try to extract data from table cells
        // Column order: Animal Name, Animal ID, Adoption Profile, Age, Weight, Days In-Care, Reason
        const name = $(cells[0]).text().trim();
        const animalId = $(cells[1]).text().trim();

        if (!animalId || !name || !/^\d+$/.test(animalId)) return;

        // Find the profile link
        let profileUrl = "";
        const link = $(cells[2]).find("a").attr("href") || $(cells[0]).find("a").attr("href");
        if (link) {
          profileUrl = link;
        } else {
          profileUrl = `${ACC_APP_BASE}${animalId}`;
        }

        const ageText = cells.length > 3 ? $(cells[3]).text().trim() : null;
        const weightText = cells.length > 4 ? $(cells[4]).text().trim() : null;
        const daysText = cells.length > 5 ? $(cells[5]).text().trim() : null;
        const reasonText = cells.length > 6 ? $(cells[6]).text().trim() : null;

        dogs.push({
          animalId,
          name,
          age: ageText,
          ageMonths: ageText ? parseAgeToMonths(ageText) : null,
          weight: weightText,
          weightLbs: weightText ? parseWeightLbs(weightText) : null,
          daysInCare: daysText ? parseInt(daysText) || null : null,
          reasonCode: reasonText || null,
          profileUrl,
          breed: null,
          sex: null,
          color: null,
          description: null,
          photoUrls: [],
          location: null,
          behaviorLevel: null,
          spayedNeutered: null,
        });
      } catch (err) {
        errors.push(`Row parse error: ${err instanceof Error ? err.message : String(err)}`);
      }
    });

    // If no dogs found in standard table, try alternate selectors
    // Ninja Tables sometimes uses different class patterns
    if (dogs.length === 0) {
      $(".ninja_table_cell, .nt_row, [data-key]").each((_i, el) => {
        const text = $(el).text().trim();
        // Try to extract animal ID from the text
        const idMatch = text.match(/\b(\d{5,7})\b/);
        if (idMatch) {
          // Found an ID-like number
          const animalId = idMatch[1];
          if (!dogs.find(d => d.animalId === animalId)) {
            dogs.push({
              animalId,
              name: text.split(/\s{2,}/)[0] || "Unknown",
              age: null,
              ageMonths: null,
              weight: null,
              weightLbs: null,
              daysInCare: null,
              reasonCode: null,
              profileUrl: `${ACC_APP_BASE}${animalId}`,
              breed: null,
              sex: null,
              color: null,
              description: null,
              photoUrls: [],
              location: null,
              behaviorLevel: null,
              spayedNeutered: null,
            });
          }
        }
      });
    }
  } catch (err) {
    errors.push(`Priority placement fetch error: ${err instanceof Error ? err.message : String(err)}`);
  }

  return {
    dogs,
    scrapedAt: new Date().toISOString(),
    source: PRIORITY_PLACEMENT_URL,
    errors,
  };
}

/**
 * Fetch detailed animal info from the ShelterBuddy public page.
 * Enriches a dog with breed, description, photos, etc.
 */
export async function fetchAnimalDetail(animalId: string): Promise<Partial<NycAccDog>> {
  const url = `${DETAIL_BASE_URL}?task=view&searchType=4&animalid=${animalId}`;

  const response = await fetch(url, {
    headers: {
      "User-Agent": "WaitingTheLongest.com/1.0 (Dog Rescue Aggregator; +https://waitingthelongest.com)",
      Accept: "text/html",
    },
  });

  if (!response.ok) {
    throw new Error(`HTTP ${response.status} fetching detail for ${animalId}`);
  }

  const html = await response.text();
  const $ = cheerio.load(html);

  const detail: Partial<NycAccDog> = {};

  // Extract breed
  const breedEl = $("td:contains('Breed')").next("td");
  if (breedEl.length) detail.breed = breedEl.text().trim() || null;

  // Extract sex
  const sexEl = $("td:contains('Sex')").next("td");
  if (sexEl.length) detail.sex = sexEl.text().trim().toLowerCase() || null;

  // Extract color
  const colorEl = $("td:contains('Color')").next("td");
  if (colorEl.length) detail.color = colorEl.text().trim() || null;

  // Extract location
  const locEl = $("td:contains('Location')").next("td");
  if (locEl.length) detail.location = locEl.text().trim() || null;

  // Extract spayed/neutered
  const snEl = $("td:contains('Spayed')").next("td");
  if (snEl.length) {
    const snText = snEl.text().trim().toLowerCase();
    detail.spayedNeutered = snText === "yes";
  }

  // Extract behavior level
  const blEl = $("td:contains('Behavior Level')").next("td");
  if (blEl.length) detail.behaviorLevel = blEl.text().trim() || null;

  // Extract description
  const descEl = $(".about-animal, .animal-description, #animalDescription");
  if (descEl.length) {
    detail.description = descEl.text().trim().replace(/\s+/g, " ") || null;
  }
  // Fallback: look for "A Little Bit About Me" section
  if (!detail.description) {
    const aboutHeader = $("h2:contains('About Me'), h3:contains('About Me'), b:contains('About Me')");
    if (aboutHeader.length) {
      const aboutSection = aboutHeader.parent().next();
      if (aboutSection.length) {
        detail.description = aboutSection.text().trim().replace(/\s+/g, " ") || null;
      }
    }
  }

  // Extract photos
  detail.photoUrls = [];
  $("img.animal-photo, .animal-photos img, .photo-gallery img").each((_i, img) => {
    const src = $(img).attr("src");
    if (src && !src.includes("no-photo") && !src.includes("placeholder")) {
      const fullUrl = src.startsWith("http") ? src : `https://nycacc.shelterbuddy.com${src}`;
      detail.photoUrls!.push(fullUrl);
    }
  });

  return detail;
}

/**
 * Calculate urgency for an NYC ACC dog based on days in care and reason codes.
 * Dogs on the priority placement list are inherently at risk.
 */
function calculateUrgency(dog: NycAccDog): {
  urgencyLevel: "critical" | "high" | "medium" | "normal";
  isOnEuthanasiaList: boolean;
} {
  // All priority placement dogs are at risk
  const isOnEuthanasiaList = true;

  // High-risk reason codes
  const criticalReasons = ["LEVEL 4", "BH", "HTS"];
  const highReasons = ["DA", "FAS", "DB"];

  if (dog.reasonCode && criticalReasons.includes(dog.reasonCode.toUpperCase())) {
    return { urgencyLevel: "critical", isOnEuthanasiaList };
  }

  if (dog.reasonCode && highReasons.includes(dog.reasonCode.toUpperCase())) {
    return { urgencyLevel: "high", isOnEuthanasiaList };
  }

  // Long stays = higher urgency
  if (dog.daysInCare && dog.daysInCare > 365) {
    return { urgencyLevel: "high", isOnEuthanasiaList };
  }

  if (dog.daysInCare && dog.daysInCare > 180) {
    return { urgencyLevel: "high", isOnEuthanasiaList };
  }

  return { urgencyLevel: "medium", isOnEuthanasiaList };
}

/**
 * Map an NYC ACC dog to our database format and upsert into the dogs table.
 * Uses external_id = `nyc-acc-{animalId}` for deduplication.
 */
export async function upsertNycAccDogs(dogs: NycAccDog[]): Promise<{
  inserted: number;
  updated: number;
  errors: number;
}> {
  const supabase = createAdminClient();
  let inserted = 0;
  let updated = 0;
  let errors = 0;
  const now = new Date().toISOString();

  // First, find or create the NYC ACC shelter records
  const shelterIds = new Map<string, string>();
  for (const [key, shelter] of Object.entries(NYC_ACC_SHELTERS)) {
    const { data: existing } = await supabase
      .from("shelters")
      .select("id")
      .ilike("name", `%${shelter.name}%`)
      .limit(1)
      .single();

    if (existing) {
      shelterIds.set(key, existing.id);
    }
  }

  // Default to manhattan if no specific shelter found
  const defaultShelterId = shelterIds.get("manhattan") || shelterIds.values().next().value;

  for (const dog of dogs) {
    try {
      const externalId = `nyc-acc-${dog.animalId}`;
      const { urgencyLevel, isOnEuthanasiaList } = calculateUrgency(dog);

      // Determine shelter based on location
      let shelterId = defaultShelterId;
      if (dog.location) {
        const loc = dog.location.toLowerCase();
        if (loc.includes("brooklyn")) shelterId = shelterIds.get("brooklyn") || shelterId;
        else if (loc.includes("staten")) shelterId = shelterIds.get("staten_island") || shelterId;
      }

      // Calculate intake date from days in care
      let intakeDate = now;
      if (dog.daysInCare) {
        const intake = new Date();
        intake.setDate(intake.getDate() - dog.daysInCare);
        intakeDate = intake.toISOString();
      }

      // Size from weight
      let size: "small" | "medium" | "large" | "xlarge" = "medium";
      if (dog.weightLbs) {
        if (dog.weightLbs < 25) size = "small";
        else if (dog.weightLbs < 50) size = "medium";
        else if (dog.weightLbs < 90) size = "large";
        else size = "xlarge";
      }

      // Age category from months
      let ageCategory: "puppy" | "young" | "adult" | "senior" = "adult";
      if (dog.ageMonths) {
        if (dog.ageMonths < 12) ageCategory = "puppy";
        else if (dog.ageMonths < 36) ageCategory = "young";
        else if (dog.ageMonths < 84) ageCategory = "adult";
        else ageCategory = "senior";
      }

      // Gender
      let gender: "male" | "female" = "male";
      if (dog.sex) {
        gender = dog.sex.toLowerCase().includes("female") ? "female" : "male";
      }

      const record = {
        name: dog.name,
        shelter_id: shelterId,
        breed_primary: dog.breed || "Mixed Breed",
        breed_mixed: true,
        size,
        age_category: ageCategory,
        age_months: dog.ageMonths,
        gender,
        color_primary: dog.color,
        weight_lbs: dog.weightLbs,
        is_spayed_neutered: dog.spayedNeutered ?? false,
        status: "available",
        is_available: true,
        intake_date: intakeDate,
        date_confidence: dog.daysInCare ? "high" as const : "medium" as const,
        date_source: "nyc_acc_priority_placement",
        description: dog.description,
        photo_urls: dog.photoUrls,
        primary_photo_url: dog.photoUrls[0] || null,
        urgency_level: urgencyLevel,
        is_on_euthanasia_list: isOnEuthanasiaList,
        euthanasia_list_added_at: now,
        external_id: externalId,
        external_source: "nyc_acc",
        external_url: dog.profileUrl,
        tags: [
          "nyc-acc",
          "priority-placement",
          dog.reasonCode ? `reason:${dog.reasonCode}` : null,
          dog.behaviorLevel ? `behavior:${dog.behaviorLevel}` : null,
        ].filter(Boolean) as string[],
        updated_at: now,
      };

      // Check if this dog already exists
      const { data: existing } = await supabase
        .from("dogs")
        .select("id")
        .eq("external_id", externalId)
        .eq("external_source", "nyc_acc")
        .limit(1)
        .single();

      if (existing) {
        // Update
        await supabase
          .from("dogs")
          .update(record)
          .eq("id", existing.id);
        updated++;
      } else {
        // Insert
        await supabase
          .from("dogs")
          .insert({ ...record, created_at: now });
        inserted++;
      }
    } catch (err) {
      errors++;
      console.error(`Error upserting NYC ACC dog ${dog.animalId}:`, err);
    }
  }

  return { inserted, updated, errors };
}

/**
 * Full NYC ACC scrape pipeline:
 * 1. Scrape priority placement list
 * 2. Fetch details for new dogs (rate limited)
 * 3. Upsert into database
 */
export async function runNycAccScrape(options?: {
  fetchDetails?: boolean;
  maxDetailFetches?: number;
  delayMs?: number;
}): Promise<{
  totalDogs: number;
  inserted: number;
  updated: number;
  detailsFetched: number;
  errors: string[];
}> {
  const { fetchDetails = true, maxDetailFetches = 20, delayMs = 2000 } = options || {};
  const allErrors: string[] = [];

  // Step 1: Scrape priority placement
  console.log("[NYC-ACC] Scraping priority placement page...");
  const result = await scrapePriorityPlacement();
  allErrors.push(...result.errors);
  console.log(`[NYC-ACC] Found ${result.dogs.length} dogs on priority placement`);

  if (result.dogs.length === 0) {
    return {
      totalDogs: 0,
      inserted: 0,
      updated: 0,
      detailsFetched: 0,
      errors: allErrors,
    };
  }

  // Step 2: Fetch details for dogs that need enrichment (rate limited)
  let detailsFetched = 0;
  if (fetchDetails) {
    const supabase = createAdminClient();
    const dogsNeedingDetails = [];

    for (const dog of result.dogs) {
      // Check if we already have details for this dog
      const { data: existing } = await supabase
        .from("dogs")
        .select("id, description")
        .eq("external_id", `nyc-acc-${dog.animalId}`)
        .eq("external_source", "nyc_acc")
        .limit(1)
        .single();

      if (!existing || !existing.description) {
        dogsNeedingDetails.push(dog);
      }
    }

    const toFetch = dogsNeedingDetails.slice(0, maxDetailFetches);
    console.log(`[NYC-ACC] Fetching details for ${toFetch.length} dogs...`);

    for (const dog of toFetch) {
      try {
        const detail = await fetchAnimalDetail(dog.animalId);
        Object.assign(dog, detail);
        detailsFetched++;

        // Rate limit: wait between requests
        if (delayMs > 0) {
          await new Promise(resolve => setTimeout(resolve, delayMs));
        }
      } catch (err) {
        allErrors.push(`Detail fetch error for ${dog.animalId}: ${err instanceof Error ? err.message : String(err)}`);
      }
    }
  }

  // Step 3: Upsert into database
  console.log(`[NYC-ACC] Upserting ${result.dogs.length} dogs into database...`);
  const upsertResult = await upsertNycAccDogs(result.dogs);

  console.log(`[NYC-ACC] Done: ${upsertResult.inserted} inserted, ${upsertResult.updated} updated, ${detailsFetched} details fetched`);

  return {
    totalDogs: result.dogs.length,
    inserted: upsertResult.inserted,
    updated: upsertResult.updated,
    detailsFetched,
    errors: allErrors,
  };
}
