/**
 * Seed shelters/organizations from multiple data sources into Supabase.
 * Handles deduplication by EIN, then name+state+city.
 *
 * Sources:
 * 1. orgs_by_state.json (40,085 IRS nonprofit orgs)
 * 2. rescuegroups_orgs.json (5,050 RescueGroups orgs)
 * 3. bf_network_orgs.json (6,012 Best Friends network orgs)
 *
 * Run: npx tsx data/seed-shelters.ts
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

const DATA_BASE = resolve(
  "C:/Users/ianme/OneDrive/Desktop/waitingthelongestdevelopment",
  "WAITINGTHELONGEST DEC 12 TRANSFER/waitingthelongestdevelopment/WaitingTheLongest/backend/data"
);

interface ShelterRecord {
  name: string;
  city: string;
  state_code: string;
  zip_code?: string;
  phone?: string;
  email?: string;
  website?: string;
  address?: string;
  shelter_type?: string;
  description?: string;
  ein?: string;
  external_id?: string;
  external_source?: string;
}

// Valid US state codes
const VALID_STATES = new Set([
  "AL","AK","AZ","AR","CA","CO","CT","DE","FL","GA",
  "HI","ID","IL","IN","IA","KS","KY","LA","ME","MD",
  "MA","MI","MN","MS","MO","MT","NE","NV","NH","NJ",
  "NM","NY","NC","ND","OH","OK","OR","PA","RI","SC",
  "SD","TN","TX","UT","VT","VA","WA","WV","WI","WY",
  "DC",
]);

function dedupKey(name: string, state: string, city: string): string {
  return `${name.toUpperCase().trim()}|${state.toUpperCase().trim()}|${city.toUpperCase().trim()}`;
}

function loadOrgsByState(): ShelterRecord[] {
  const path = resolve(DATA_BASE, "master_lists/orgs_by_state.json");
  console.log("Loading orgs_by_state.json...");
  const data = JSON.parse(readFileSync(path, "utf-8"));
  const records: ShelterRecord[] = [];

  for (const [state, orgs] of Object.entries(data.by_state)) {
    if (!VALID_STATES.has(state.toUpperCase())) continue;
    for (const org of orgs as Record<string, unknown>[]) {
      records.push({
        name: (org.name as string) || "Unknown",
        city: (org.city as string) || "",
        state_code: state.toUpperCase(),
        zip_code: (org.zip_code as string) || undefined,
        phone: (org.phone as string) || undefined,
        email: (org.email as string) || undefined,
        website: (org.website as string) || undefined,
        address: (org.street as string) || undefined,
        shelter_type: "rescue", // Default for IRS nonprofits
        ein: (org.ein as string) || undefined,
        external_source: "irs_990",
      });
    }
  }
  console.log(`  Loaded ${records.length} orgs from IRS data`);
  return records;
}

function loadRescueGroupsOrgs(): ShelterRecord[] {
  const path = resolve(DATA_BASE, "rescuegroups_orgs.json");
  console.log("Loading rescuegroups_orgs.json...");
  const data = JSON.parse(readFileSync(path, "utf-8")) as Record<string, unknown>[];
  const records: ShelterRecord[] = [];

  for (const org of data) {
    const state = ((org.state as string) || "").toUpperCase();
    if (!VALID_STATES.has(state)) continue;

    records.push({
      name: (org.name as string) || "Unknown",
      city: (org.city as string) || "",
      state_code: state,
      zip_code: (org.zip_code as string) || undefined,
      phone: (org.phone as string) || undefined,
      email: (org.email as string) || undefined,
      website: (org.website as string) || undefined,
      address: (org.address as string) || undefined,
      shelter_type: ((org.org_type as string) || "rescue").toLowerCase(),
      description: (org.description as string) || undefined,
      external_id: (org.external_id as string) || undefined,
      external_source: "rescuegroups",
    });
  }
  console.log(`  Loaded ${records.length} orgs from RescueGroups`);
  return records;
}

function loadBestFriendsOrgs(): ShelterRecord[] {
  const path = resolve(DATA_BASE, "bf_network_orgs.json");
  console.log("Loading bf_network_orgs.json...");
  const data = JSON.parse(readFileSync(path, "utf-8")) as Record<string, unknown>[];
  const records: ShelterRecord[] = [];

  for (const org of data) {
    const state = ((org.state as string) || "").toUpperCase();
    if (!VALID_STATES.has(state)) continue;

    records.push({
      name: (org.name as string) || "Unknown",
      city: (org.city as string) || "",
      state_code: state,
      website: (org.website as string) || (org.profile_url as string) || undefined,
      description: (org.description as string) || undefined,
      shelter_type: "rescue",
      external_source: "best_friends",
    });
  }
  console.log(`  Loaded ${records.length} orgs from Best Friends`);
  return records;
}

async function main() {
  console.log("=== Shelter/Organization Import ===\n");

  // Load all sources
  const irsOrgs = loadOrgsByState();
  const rgOrgs = loadRescueGroupsOrgs();
  const bfOrgs = loadBestFriendsOrgs();

  // Deduplicate: IRS data is the primary source (has EIN)
  // Then merge in RescueGroups and Best Friends data
  const seen = new Map<string, ShelterRecord>();
  const einSeen = new Set<string>();

  // Process IRS orgs first (primary source)
  for (const org of irsOrgs) {
    if (org.ein) {
      if (einSeen.has(org.ein)) continue;
      einSeen.add(org.ein);
    }
    const key = dedupKey(org.name, org.state_code, org.city);
    if (!seen.has(key)) {
      seen.set(key, org);
    }
  }
  console.log(`After IRS dedup: ${seen.size} unique orgs`);

  // Merge RescueGroups (enriches existing or adds new)
  let rgNew = 0;
  let rgMerged = 0;
  for (const org of rgOrgs) {
    const key = dedupKey(org.name, org.state_code, org.city);
    const existing = seen.get(key);
    if (existing) {
      // Enrich: fill in missing fields from RescueGroups
      if (!existing.phone && org.phone) existing.phone = org.phone;
      if (!existing.email && org.email) existing.email = org.email;
      if (!existing.website && org.website) existing.website = org.website;
      if (!existing.description && org.description) existing.description = org.description;
      if (org.external_id) {
        existing.external_id = org.external_id;
        existing.external_source = "rescuegroups";
      }
      rgMerged++;
    } else {
      seen.set(key, org);
      rgNew++;
    }
  }
  console.log(`RescueGroups: ${rgNew} new, ${rgMerged} merged. Total: ${seen.size}`);

  // Merge Best Friends
  let bfNew = 0;
  for (const org of bfOrgs) {
    const key = dedupKey(org.name, org.state_code, org.city);
    if (!seen.has(key)) {
      seen.set(key, org);
      bfNew++;
    }
  }
  console.log(`Best Friends: ${bfNew} new. Total: ${seen.size}`);

  // Prepare for insertion
  const shelters = Array.from(seen.values()).map((s) => ({
    name: s.name.substring(0, 255),
    city: (s.city || "").substring(0, 100),
    state_code: s.state_code,
    zip_code: s.zip_code || null,
    phone: s.phone || null,
    email: s.email || null,
    website: s.website ? s.website.substring(0, 500) : null,
    address: s.address || null,
    shelter_type: mapShelterType(s.shelter_type),
    description: s.description ? s.description.substring(0, 5000) : null,
    ein: s.ein || null,
    external_id: s.external_id || null,
    external_source: s.external_source || null,
    is_verified: false,
  }));

  console.log(`\nInserting ${shelters.length} shelters in batches...`);

  // Insert in batches of 500
  const BATCH_SIZE = 500;
  let inserted = 0;
  let errors = 0;

  for (let i = 0; i < shelters.length; i += BATCH_SIZE) {
    const batch = shelters.slice(i, i + BATCH_SIZE);
    const { error } = await supabase
      .from("shelters")
      .upsert(batch, {
        onConflict: "name,state_code",
        ignoreDuplicates: true,
      });

    if (error) {
      console.error(`  Batch ${Math.floor(i / BATCH_SIZE) + 1} error: ${error.message}`);
      errors++;
    } else {
      inserted += batch.length;
      if ((i / BATCH_SIZE) % 10 === 0) {
        console.log(`  Progress: ${inserted}/${shelters.length} (${Math.round((inserted / shelters.length) * 100)}%)`);
      }
    }
  }

  console.log(`\n=== Done! ===`);
  console.log(`Total inserted/updated: ${inserted}`);
  console.log(`Batch errors: ${errors}`);
  console.log(`Unique organizations: ${shelters.length}`);
}

function mapShelterType(type?: string): string {
  if (!type) return "rescue";
  const lower = type.toLowerCase();
  if (lower === "municipal") return "municipal";
  if (lower.includes("shelter") || lower.includes("private")) return "private";
  if (lower.includes("humane")) return "humane_society";
  if (lower.includes("spca")) return "spca";
  if (lower.includes("foster")) return "foster_network";
  return "rescue";
}

main();
