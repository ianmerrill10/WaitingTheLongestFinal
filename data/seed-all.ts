/**
 * Master seed script - runs all seed scripts in order.
 * Run: npx tsx data/seed-all.ts
 *
 * Prerequisites:
 * 1. Create a Supabase project at https://supabase.com
 * 2. Run the SQL migrations in supabase/migrations/ (001-006)
 * 3. Set NEXT_PUBLIC_SUPABASE_URL and SUPABASE_SERVICE_ROLE_KEY in .env.local
 *
 * Order: states → breeds → shelters (shelters depends on states)
 */
import { execSync } from "child_process";

const scripts = [
  { name: "States (50)", file: "data/seed-states.ts" },
  { name: "Breeds (170+)", file: "data/seed-breeds.ts" },
  { name: "Shelters (40,000+)", file: "data/seed-shelters.ts" },
];

console.log("====================================");
console.log("WaitingTheLongest.com - Database Seed");
console.log("====================================\n");

for (const script of scripts) {
  console.log(`\n--- Seeding: ${script.name} ---`);
  try {
    execSync(`npx tsx ${script.file}`, {
      stdio: "inherit",
      cwd: process.cwd(),
    });
  } catch {
    console.error(`FAILED: ${script.name}`);
    process.exit(1);
  }
}

console.log("\n====================================");
console.log("All seed data imported successfully!");
console.log("====================================");
console.log("\nNext steps:");
console.log("1. Verify data: SELECT count(*) FROM shelters;");
console.log("2. Configure RescueGroups API key in .env.local");
console.log("3. Trigger dog sync: /api/cron/sync-dogs");
console.log("4. Deploy to Vercel: vercel deploy");
