/**
 * Export all dogs to CSV files — one per state + master file.
 *
 * Usage: npx tsx --tsconfig tsconfig.json scripts/export-dogs.ts
 */
import dotenv from "dotenv";
import path from "path";
import fs from "fs";
dotenv.config({ path: path.join(__dirname, "..", ".env.local") });

import { createAdminClient } from "../src/lib/supabase/admin";

const EXPORTS_DIR = path.join(__dirname, "..", "exports");
const DOGS_DIR = path.join(EXPORTS_DIR, "dogs");

const COLUMNS = [
  "id", "name", "shelter_name", "shelter_city", "shelter_state",
  "breed_primary", "breed_secondary", "breed_mixed", "size", "age_category",
  "age_months", "gender", "color_primary", "weight_lbs",
  "is_spayed_neutered", "status", "is_available",
  "intake_date", "date_confidence", "date_source", "wait_days",
  "urgency_level", "euthanasia_date", "is_on_euthanasia_list",
  "good_with_kids", "good_with_dogs", "good_with_cats", "house_trained",
  "has_special_needs", "adoption_fee",
  "photo_count", "primary_photo_url", "external_url",
  "external_source", "verification_status",
  "created_at", "updated_at"
];

function escapeCSV(val: unknown): string {
  if (val === null || val === undefined) return "";
  const s = String(val);
  if (s.includes(",") || s.includes('"') || s.includes("\n")) {
    return `"${s.replace(/"/g, '""')}"`;
  }
  return s;
}

function writeCSV(filePath: string, rows: Record<string, unknown>[]) {
  const header = COLUMNS.join(",") + "\n";
  const lines = rows.map(row =>
    COLUMNS.map(col => escapeCSV(row[col])).join(",")
  ).join("\n");
  fs.writeFileSync(filePath, header + lines, "utf-8");
}

async function main() {
  const supabase = createAdminClient();

  fs.mkdirSync(DOGS_DIR, { recursive: true });

  console.log("Fetching all dogs with shelter info...\n");

  const allDogs: Record<string, unknown>[] = [];
  let offset = 0;
  const pageSize = 1000;
  let hasMore = true;

  while (hasMore) {
    const { data, error } = await supabase
      .from("dogs")
      .select(`
        id, name, breed_primary, breed_secondary, breed_mixed, size,
        age_category, age_months, gender, color_primary, weight_lbs,
        is_spayed_neutered, status, is_available,
        intake_date, date_confidence, date_source,
        urgency_level, euthanasia_date, is_on_euthanasia_list,
        good_with_kids, good_with_dogs, good_with_cats, house_trained,
        has_special_needs, adoption_fee,
        photo_urls, primary_photo_url, external_url,
        external_source, verification_status,
        created_at, updated_at,
        shelter:shelters!dogs_shelter_id_fkey!inner(name, city, state_code)
      `)
      .eq("is_available", true)
      .range(offset, offset + pageSize - 1);

    if (error) {
      console.error(`Error at offset ${offset}:`, error.message);
      break;
    }

    if (!data || data.length === 0) {
      hasMore = false;
    } else {
      for (const dog of data) {
        const shelter = (dog as unknown as Record<string, unknown>).shelter as Record<string, unknown> | null;
        const intakeDate = dog.intake_date ? new Date(dog.intake_date as string) : null;
        const waitDays = intakeDate
          ? Math.floor((Date.now() - intakeDate.getTime()) / 86400000)
          : null;

        allDogs.push({
          ...dog,
          shelter_name: shelter?.name || "",
          shelter_city: shelter?.city || "",
          shelter_state: shelter?.state_code || "",
          wait_days: waitDays,
          photo_count: Array.isArray(dog.photo_urls) ? (dog.photo_urls as unknown[]).length : 0,
        });
      }
      offset += pageSize;
      process.stdout.write(`\r  Fetched ${allDogs.length} dogs...`);
      if (data.length < pageSize) hasMore = false;
    }
  }

  console.log(`\n  Total: ${allDogs.length} available dogs\n`);

  // Group by state
  const byState = new Map<string, Record<string, unknown>[]>();
  for (const dog of allDogs) {
    const state = (dog.shelter_state as string) || "UNKNOWN";
    if (!byState.has(state)) byState.set(state, []);
    byState.get(state)!.push(dog);
  }

  // Write state files
  console.log("Writing state CSV files...");
  const stateSummary: { state: string; count: number; waitAvg: number }[] = [];

  for (const [stateCode, dogs] of Array.from(byState.entries())) {
    // Sort by wait_days descending (longest waiting first)
    dogs.sort((a, b) => ((b.wait_days as number) || 0) - ((a.wait_days as number) || 0));

    const fileName = `${stateCode}_dogs.csv`;
    const filePath = path.join(DOGS_DIR, fileName);
    writeCSV(filePath, dogs);

    const avgWait = dogs.reduce((sum, d) => sum + ((d.wait_days as number) || 0), 0) / dogs.length;
    stateSummary.push({ state: stateCode, count: dogs.length, waitAvg: Math.round(avgWait) });
    console.log(`  ${stateCode}: ${dogs.length} dogs (avg wait: ${Math.round(avgWait)} days) → ${fileName}`);
  }

  // Write master file (sorted by wait_days descending — longest waiting dogs first)
  console.log("\nWriting master CSV file...");
  allDogs.sort((a, b) => ((b.wait_days as number) || 0) - ((a.wait_days as number) || 0));
  const masterPath = path.join(EXPORTS_DIR, "ALL_DOGS_master.csv");
  writeCSV(masterPath, allDogs);
  console.log(`  Master: ${allDogs.length} dogs → ALL_DOGS_master.csv`);

  // Summary
  console.log("\n═══════════════════════════════════");
  console.log("  DOG EXPORT SUMMARY");
  console.log("═══════════════════════════════════");
  stateSummary.sort((a, b) => b.count - a.count);
  for (const s of stateSummary) {
    console.log(`  ${s.state}: ${s.count.toLocaleString()} dogs (avg ${s.waitAvg} day wait)`);
  }
  console.log(`\n  TOTAL: ${allDogs.length.toLocaleString()} dogs across ${byState.size} states`);
  console.log(`  Files: exports/dogs/ (per-state) + exports/ALL_DOGS_master.csv`);
}

main().catch(console.error);
