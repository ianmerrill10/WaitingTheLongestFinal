/**
 * One-time script: Classify all shelter platforms.
 * Visits each shelter website, detects the platform (shelterluv, rescuegroups, etc.),
 * and updates the database.
 *
 * Usage: npx tsx --tsconfig tsconfig.json scripts/classify-shelters.ts
 */
import dotenv from "dotenv";
import path from "path";
dotenv.config({ path: path.join(__dirname, "..", ".env.local") });

import { createAdminClient } from "../src/lib/supabase/admin";
import { detectPlatform } from "../src/lib/scrapers/platform-detector";

const BATCH_SIZE = 20;
const CONCURRENT = 5;

async function main() {
  const supabase = createAdminClient();

  // Count shelters with websites but unknown platform
  const { count } = await supabase
    .from("shelters")
    .select("id", { count: "exact", head: true })
    .not("website", "is", null)
    .neq("website", "")
    .eq("website_platform", "unknown");

  console.log(`\nShelters to classify: ${count}\n`);
  if (!count) {
    console.log("All shelters already classified.");
    return;
  }

  let classified = 0;
  let offset = 0;

  while (offset < (count || 0)) {
    const { data: shelters } = await supabase
      .from("shelters")
      .select("id, name, website, state_code")
      .not("website", "is", null)
      .neq("website", "")
      .eq("website_platform", "unknown")
      .range(offset, offset + BATCH_SIZE - 1);

    if (!shelters || shelters.length === 0) break;

    // Process in chunks of CONCURRENT
    for (let i = 0; i < shelters.length; i += CONCURRENT) {
      const chunk = shelters.slice(i, i + CONCURRENT);
      const results = await Promise.allSettled(
        chunk.map(async (shelter) => {
          try {
            const result = await detectPlatform(shelter.website!);
            const update: Record<string, unknown> = {
              website_platform: result.platform,
            };
            if (result.platformId) update.platform_shelter_id = result.platformId;
            if (result.adoptablePageUrl) update.adoptable_page_url = result.adoptablePageUrl;

            await supabase.from("shelters").update(update).eq("id", shelter.id);
            classified++;
            console.log(
              `[${classified}/${count}] ${shelter.state_code} | ${shelter.name}: ${result.platform}${result.platformId ? ` (${result.platformId})` : ""}`
            );
          } catch (err) {
            console.error(`  ERROR classifying ${shelter.name}: ${(err as Error).message}`);
          }
        })
      );
    }

    offset += BATCH_SIZE;
  }

  console.log(`\nDone! Classified ${classified} shelters.`);
}

main().catch(console.error);
