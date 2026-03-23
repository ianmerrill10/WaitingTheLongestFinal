/**
 * Export all shelters/rescues to CSV files.
 * - 50 state-specific CSV files in exports/states/
 * - 1 master CSV with all states in exports/
 *
 * Run with: npx tsx scripts/export-shelters.ts
 */
import { createClient } from "@supabase/supabase-js";
import * as dotenv from "dotenv";
import * as path from "path";
import * as fs from "fs";

dotenv.config({ path: path.resolve(__dirname, "../.env.local") });

const supabaseUrl = process.env.NEXT_PUBLIC_SUPABASE_URL!;
const serviceKey = process.env.SUPABASE_SERVICE_ROLE_KEY!;
const supabase = createClient(supabaseUrl, serviceKey);

const EXPORTS_DIR = path.resolve(__dirname, "../exports");
const STATES_DIR = path.resolve(EXPORTS_DIR, "states");

const US_STATES = [
  "AL","AK","AZ","AR","CA","CO","CT","DE","FL","GA",
  "HI","ID","IL","IN","IA","KS","KY","LA","ME","MD",
  "MA","MI","MN","MS","MO","MT","NE","NV","NH","NJ",
  "NM","NY","NC","ND","OH","OK","OR","PA","RI","SC",
  "SD","TN","TX","UT","VT","VA","WA","WV","WI","WY",
];

const STATE_NAMES: Record<string, string> = {
  AL:"Alabama",AK:"Alaska",AZ:"Arizona",AR:"Arkansas",CA:"California",
  CO:"Colorado",CT:"Connecticut",DE:"Delaware",FL:"Florida",GA:"Georgia",
  HI:"Hawaii",ID:"Idaho",IL:"Illinois",IN:"Indiana",IA:"Iowa",
  KS:"Kansas",KY:"Kentucky",LA:"Louisiana",ME:"Maine",MD:"Maryland",
  MA:"Massachusetts",MI:"Michigan",MN:"Minnesota",MS:"Mississippi",MO:"Missouri",
  MT:"Montana",NE:"Nebraska",NV:"Nevada",NH:"New Hampshire",NJ:"New Jersey",
  NM:"New Mexico",NY:"New York",NC:"North Carolina",ND:"North Dakota",OH:"Ohio",
  OK:"Oklahoma",OR:"Oregon",PA:"Pennsylvania",RI:"Rhode Island",SC:"South Carolina",
  SD:"South Dakota",TN:"Tennessee",TX:"Texas",UT:"Utah",VT:"Vermont",
  VA:"Virginia",WA:"Washington",WV:"West Virginia",WI:"Wisconsin",WY:"Wyoming",
};

const CSV_HEADERS = [
  "name",
  "shelter_type",
  "city",
  "state_code",
  "state_name",
  "zip_code",
  "county",
  "phone",
  "email",
  "website",
  "facebook_url",
  "description",
  "is_kill_shelter",
  "is_verified",
  "is_active",
  "total_animals",
  "ein",
  "external_id",
  "external_source",
  "integration_type",
  "partner_status",
  "partner_tier",
  "partner_since",
  "feed_url",
  "last_synced_at",
  "last_feed_sync_at",
  "latitude",
  "longitude",
  "social_facebook",
  "social_instagram",
  "social_tiktok",
  "social_youtube",
  "response_time_hours",
  "adoption_success_rate",
  "created_at",
  "updated_at",
];

function escapeCSV(val: unknown): string {
  if (val === null || val === undefined) return "";
  const str = String(val);
  if (str.includes(",") || str.includes('"') || str.includes("\n") || str.includes("\r")) {
    return `"${str.replace(/"/g, '""')}"`;
  }
  return str;
}

function shelterToRow(shelter: Record<string, unknown>): string {
  return CSV_HEADERS.map((h) => {
    if (h === "state_name") {
      return escapeCSV(STATE_NAMES[shelter.state_code as string] || "");
    }
    return escapeCSV(shelter[h]);
  }).join(",");
}

async function fetchAllShelters(): Promise<Record<string, unknown>[]> {
  const allShelters: Record<string, unknown>[] = [];
  const pageSize = 1000;
  let offset = 0;
  let hasMore = true;

  console.log("Fetching all shelters from database...");

  while (hasMore) {
    const { data, error } = await supabase
      .from("shelters")
      .select(CSV_HEADERS.filter(h => h !== "state_name").join(","))
      .order("state_code", { ascending: true })
      .order("name", { ascending: true })
      .range(offset, offset + pageSize - 1);

    if (error) {
      console.error(`Error fetching shelters at offset ${offset}:`, error.message);
      break;
    }

    if (!data || data.length === 0) {
      hasMore = false;
    } else {
      allShelters.push(...(data as unknown as Record<string, unknown>[]));
      offset += pageSize;
      process.stdout.write(`\r  Fetched ${allShelters.length} shelters...`);
      if (data.length < pageSize) hasMore = false;
    }
  }

  console.log(`\n  Total: ${allShelters.length} shelters\n`);
  return allShelters;
}

function writeCSV(filePath: string, shelters: Record<string, unknown>[]): void {
  const header = CSV_HEADERS.join(",");
  const rows = shelters.map(shelterToRow);
  fs.writeFileSync(filePath, header + "\n" + rows.join("\n") + "\n", "utf-8");
}

async function main() {
  console.log("=== WaitingTheLongest.com — Shelter/Rescue CSV Export ===\n");

  // Ensure directories exist
  fs.mkdirSync(STATES_DIR, { recursive: true });

  // Fetch all shelters
  const allShelters = await fetchAllShelters();

  if (allShelters.length === 0) {
    console.error("No shelters found!");
    return;
  }

  // Group by state
  const byState = new Map<string, Record<string, unknown>[]>();
  for (const s of US_STATES) {
    byState.set(s, []);
  }
  let unmatched = 0;
  for (const shelter of allShelters) {
    const state = shelter.state_code as string;
    if (byState.has(state)) {
      byState.get(state)!.push(shelter);
    } else {
      unmatched++;
    }
  }

  // Write state files
  console.log("Writing state CSV files...");
  const stateSummary: { state: string; name: string; count: number }[] = [];

  for (const [stateCode, shelters] of Array.from(byState.entries())) {
    const stateName = STATE_NAMES[stateCode] || stateCode;
    const fileName = `${stateCode}_${stateName.replace(/ /g, "_")}_shelters.csv`;
    const filePath = path.join(STATES_DIR, fileName);
    writeCSV(filePath, shelters);
    stateSummary.push({ state: stateCode, name: stateName, count: shelters.length });
    console.log(`  ${stateCode} (${stateName}): ${shelters.length} shelters → ${fileName}`);
  }

  // Write master file
  console.log("\nWriting master CSV file...");
  const masterPath = path.join(EXPORTS_DIR, "ALL_STATES_shelters_master.csv");
  writeCSV(masterPath, allShelters);
  console.log(`  Master file: ${allShelters.length} shelters → ALL_STATES_shelters_master.csv`);

  // Write summary
  console.log("\n=== SUMMARY ===");
  console.log(`Total shelters exported: ${allShelters.length}`);
  console.log(`State files: ${stateSummary.length}`);
  console.log(`Unmatched (non-US state codes): ${unmatched}`);
  console.log(`\nTop 10 states by shelter count:`);
  stateSummary.sort((a, b) => b.count - a.count);
  for (const s of stateSummary.slice(0, 10)) {
    console.log(`  ${s.state} (${s.name}): ${s.count}`);
  }
  console.log(`\nBottom 5 states by shelter count:`);
  for (const s of stateSummary.slice(-5)) {
    console.log(`  ${s.state} (${s.name}): ${s.count}`);
  }

  console.log(`\nFiles written to:`);
  console.log(`  State files: ${STATES_DIR}/`);
  console.log(`  Master file: ${masterPath}`);
}

main().catch(console.error);
