/**
 * Seed dog breeds from dog_breeds.json into Supabase.
 * Run: npx tsx data/seed-breeds.ts
 */
import { createClient } from "@supabase/supabase-js";
import { readFileSync } from "fs";
import { resolve } from "path";
import * as dotenv from "dotenv";
dotenv.config({ path: ".env.local" });

const supabase = createClient(
  process.env.NEXT_PUBLIC_SUPABASE_URL!,
  process.env.SUPABASE_SERVICE_ROLE_KEY!
);

// Path to dog_breeds.json from the development project
const BREEDS_PATH = resolve(
  "C:/Users/ianme/OneDrive/Desktop/waitingthelongestdevelopment",
  "WAITINGTHELONGEST DEC 12 TRANSFER/waitingthelongestdevelopment/WaitingTheLongest/backend/data/dog_breeds.json"
);

interface BreedData {
  id: number;
  name: string;
  breed_group: string;
  height?: { imperial: string; metric: string };
  weight?: { imperial: string; metric: string };
  life_span: string;
  temperament: string;
  origin?: string;
  bred_for?: string;
  image_url?: string;
  description?: string;
  exercise_needs?: string;
  grooming_needs?: string;
  good_with_kids?: boolean;
  good_with_dogs?: boolean;
  good_with_cats?: boolean;
  apartment_friendly?: boolean;
}

async function main() {
  console.log("Loading breeds from", BREEDS_PATH);

  let breeds: BreedData[];
  try {
    breeds = JSON.parse(readFileSync(BREEDS_PATH, "utf-8"));
  } catch {
    console.error("Could not read breeds file. Check the path.");
    process.exit(1);
  }

  console.log(`Found ${breeds.length} breeds. Upserting...`);

  // Map to our schema (match breeds table columns from 004_breeds.sql)
  const mapped = breeds.map((b) => {
    // Parse weight range like "6 - 13" into min/max
    const weightParts = b.weight?.imperial?.split("-").map((s) => parseInt(s.trim()));
    const weightMin = weightParts?.[0] && !isNaN(weightParts[0]) ? weightParts[0] : null;
    const weightMax = weightParts?.[1] && !isNaN(weightParts[1]) ? weightParts[1] : weightMin;

    // Parse life span like "10 - 12 years"
    const lifeParts = b.life_span?.match(/(\d+)\s*-?\s*(\d+)?/);
    const lifeMin = lifeParts?.[1] ? parseInt(lifeParts[1]) : null;
    const lifeMax = lifeParts?.[2] ? parseInt(lifeParts[2]) : lifeMin;

    return {
      name: b.name,
      breed_group: b.breed_group || null,
      temperament: b.temperament || null,
      good_with_children: b.good_with_kids ?? null,
      good_with_other_dogs: b.good_with_dogs ?? null,
      good_with_cats: b.good_with_cats ?? null,
      apartment_suitable: b.apartment_friendly ?? null,
      exercise_needs: b.exercise_needs || null,
      grooming_needs: b.grooming_needs || null,
      weight_min_lbs: weightMin,
      weight_max_lbs: weightMax,
      life_expectancy_min: lifeMin,
      life_expectancy_max: lifeMax,
      description: b.description || null,
      image_url: b.image_url || null,
    };
  });

  // Batch upsert (50 at a time)
  const BATCH_SIZE = 50;
  let inserted = 0;
  let errors = 0;

  for (let i = 0; i < mapped.length; i += BATCH_SIZE) {
    const batch = mapped.slice(i, i + BATCH_SIZE);
    const { error } = await supabase
      .from("breeds")
      .upsert(batch, { onConflict: "name" });

    if (error) {
      console.error(`Error at batch ${i}: ${error.message}`);
      errors++;
    } else {
      inserted += batch.length;
    }
  }

  console.log(`Done! ${inserted} breeds inserted/updated, ${errors} batch errors.`);
}

main();
