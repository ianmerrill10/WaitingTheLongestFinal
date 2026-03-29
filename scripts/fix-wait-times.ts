/**
 * One-time script to fix unreasonable wait times and remove non-dog listings.
 * Run with: npx tsx scripts/fix-wait-times.ts
 */
import { createClient } from "@supabase/supabase-js";
import * as dotenv from "dotenv";
import * as path from "path";

dotenv.config({ path: path.resolve(__dirname, "../.env.local") });

const supabaseUrl = process.env.NEXT_PUBLIC_SUPABASE_URL!;
const serviceKey = process.env.SUPABASE_SERVICE_ROLE_KEY!;
const supabase = createClient(supabaseUrl, serviceKey);

const MAX_WAIT_DAYS: Record<string, number> = {
  verified: 1825,
  high: 1825,
  medium: 1095,
  low: 730,
  unknown: 365,
};

const NON_DOG_PATTERNS = [
  /^fosters?\s+needed$/i,
  /^foster\s+(?:dog\s+)?blog$/i,
  /^(?:pre-?approval|pre-?approve|application)$/i,
  /^sponsor\b/i,
  /\bsponsorship\b/i,
  /^donate\b/i,
  /^donation/i,
  /^volunteer/i,
  /^about\s+(?:us|our)/i,
  /^how\s+to\s+(?:adopt|foster|help)/i,
  /^(?:wish\s*list|wish-?list)$/i,
  /^(?:supplies|items)\s+needed$/i,
  /^(?:info|information|faq|contact)/i,
  /^(?:event|fundraiser|gala|auction)/i,
  /^(?:memorial|in\s+memory|rainbow\s+bridge)/i,
  /^(?:happy\s+tail|success\s+stor)/i,
  /^(?:adoption\s+(?:process|info|application|fee))/i,
  /^(?:our\s+(?:dogs|mission|story|team))/i,
  /^(?:foster\s+(?:info|application|program|home))/i,
  /^(?:transport|courtesy\s+(?:post|listing))/i,
  /^FOHA\s/i,
];

async function main() {
  const now = new Date();
  let capped = 0;
  let removed = 0;
  let errors = 0;

  console.log("=== Fix Wait Times & Remove Non-Dog Listings ===\n");

  // Step 1: Remove non-dog listings
  console.log("Step 1: Scanning for non-dog listings...");
  const { data: allDogs } = await supabase
    .from("dogs")
    .select("id, name")
    .eq("is_available", true);

  if (allDogs) {
    const nonDogIds: string[] = [];
    for (const dog of allDogs) {
      if (NON_DOG_PATTERNS.some(p => p.test(dog.name || ""))) {
        nonDogIds.push(dog.id);
        console.log(`  [REMOVE] "${dog.name}"`);
      }
    }

    if (nonDogIds.length > 0) {
      const { error } = await supabase
        .from("dogs")
        .update({ is_available: false, status: "removed" })
        .in("id", nonDogIds);

      if (error) {
        console.error(`  Error: ${error.message}`);
        errors++;
      } else {
        removed = nonDogIds.length;
        console.log(`  Removed ${removed} non-dog listings\n`);
      }
    } else {
      console.log("  No non-dog listings found\n");
    }
  }

  // Step 2: Cap unreasonable wait times
  console.log("Step 2: Capping unreasonable wait times...");
  for (const [confidence, maxDays] of Object.entries(MAX_WAIT_DAYS)) {
    const cutoffDate = new Date(now);
    cutoffDate.setDate(cutoffDate.getDate() - maxDays);
    const cappedDateStr = cutoffDate.toISOString();

    const { data: stale } = await supabase
      .from("dogs")
      .select("id, name, intake_date, original_intake_date, date_source, date_confidence")
      .eq("is_available", true)
      .eq("date_confidence", confidence)
      .lt("intake_date", cappedDateStr);

    if (!stale || stale.length === 0) {
      console.log(`  [${confidence}] No dogs exceed ${maxDays}-day cap`);
      continue;
    }

    console.log(`  [${confidence}] Found ${stale.length} dogs exceeding ${maxDays}-day cap:`);

    for (const dog of stale) {
      const oldDate = dog.intake_date?.substring(0, 10);
      const newDate = cappedDateStr.substring(0, 10);
      const newConfidence = confidence === "verified" ? "medium" : "low";

      const { error } = await supabase
        .from("dogs")
        .update({
          intake_date: cappedDateStr,
          original_intake_date: dog.original_intake_date || dog.intake_date,
          date_confidence: newConfidence,
          date_source: `${dog.date_source || "unknown"}|wait_capped_${maxDays}d`,
          ranking_eligible: false,
          intake_date_observation_count: 1,
        })
        .eq("id", dog.id);

      if (error) {
        errors++;
        console.error(`    Error updating "${dog.name}": ${error.message}`);
      } else {
        capped++;
        console.log(`    "${dog.name}": ${oldDate} → ${newDate} (${confidence} → ${newConfidence})`);
      }
    }
  }

  // Step 3: Show new top 10 longest waiting
  console.log("\n=== New Top 10 Longest Waiting ===");
  const { data: top10 } = await supabase
    .from("dogs")
    .select("name, intake_date, date_confidence, date_source")
    .eq("is_available", true)
    .in("date_confidence", ["verified", "high", "medium"])
    .order("intake_date", { ascending: true })
    .limit(10);

  if (top10) {
    for (const dog of top10) {
      const waitDays = Math.floor((now.getTime() - new Date(dog.intake_date).getTime()) / 86400000);
      const years = Math.floor(waitDays / 365);
      const days = waitDays % 365;
      console.log(`  ${dog.name}: ${waitDays} days (${years}y ${days}d) [${dog.date_confidence}] ${dog.date_source}`);
    }
  }

  console.log(`\n=== Summary ===`);
  console.log(`  Wait times capped: ${capped}`);
  console.log(`  Non-dogs removed: ${removed}`);
  console.log(`  Errors: ${errors}`);
}

main().catch(console.error);
