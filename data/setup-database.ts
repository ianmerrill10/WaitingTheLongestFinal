/**
 * Database setup script - runs migrations against Supabase
 *
 * Usage:
 *   npx tsx data/setup-database.ts
 *
 * Prerequisites:
 *   - Set NEXT_PUBLIC_SUPABASE_URL and SUPABASE_SERVICE_ROLE_KEY in .env.local
 *   - Or run the SQL in supabase/setup.sql directly in the Supabase SQL Editor
 */

import * as dotenv from "dotenv";
import * as fs from "fs";
import * as path from "path";

dotenv.config({ path: ".env.local" });

const SUPABASE_URL = process.env.NEXT_PUBLIC_SUPABASE_URL;
const SERVICE_ROLE_KEY = process.env.SUPABASE_SERVICE_ROLE_KEY;

if (!SUPABASE_URL || SUPABASE_URL.includes("your_supabase")) {
  console.error("❌ NEXT_PUBLIC_SUPABASE_URL is not set or is a placeholder.");
  console.error("   Please update .env.local with your Supabase project URL.");
  console.error("   Get it from: https://supabase.com/dashboard → Settings → API");
  process.exit(1);
}

if (!SERVICE_ROLE_KEY || SERVICE_ROLE_KEY.includes("your_supabase")) {
  console.error("❌ SUPABASE_SERVICE_ROLE_KEY is not set or is a placeholder.");
  console.error("   Please update .env.local with your service role key.");
  console.error("   Get it from: https://supabase.com/dashboard → Settings → API → service_role");
  process.exit(1);
}

async function runSQL(sql: string, label: string): Promise<void> {
  console.log(`\n📋 Running: ${label}...`);

  const response = await fetch(`${SUPABASE_URL}/rest/v1/rpc`, {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      apikey: SERVICE_ROLE_KEY!,
      Authorization: `Bearer ${SERVICE_ROLE_KEY}`,
    },
    body: JSON.stringify({
      query: sql,
    }),
  });

  // The REST API doesn't support raw SQL. Use the management API instead.
  // Actually, we'll use the pg endpoint or run SQL via supabase-js.
  // For now, let's use a different approach.
  if (!response.ok) {
    const text = await response.text();
    console.log(`   ⚠️  REST API doesn't support raw SQL: ${text.slice(0, 200)}`);
    return;
  }
}

async function main() {
  console.log("🚀 WaitingTheLongest.com - Database Setup");
  console.log("=".repeat(50));
  console.log(`📍 Supabase URL: ${SUPABASE_URL}`);

  // Read the consolidated setup SQL
  const setupSqlPath = path.join(__dirname, "..", "supabase", "setup.sql");

  if (!fs.existsSync(setupSqlPath)) {
    console.error("❌ Could not find supabase/setup.sql");
    process.exit(1);
  }

  const setupSQL = fs.readFileSync(setupSqlPath, "utf-8");
  console.log(`\n📄 Loaded setup.sql (${(setupSQL.length / 1024).toFixed(1)} KB)`);

  // Test connection first
  console.log("\n🔌 Testing Supabase connection...");
  try {
    const testResponse = await fetch(`${SUPABASE_URL}/rest/v1/`, {
      headers: {
        apikey: SERVICE_ROLE_KEY!,
        Authorization: `Bearer ${SERVICE_ROLE_KEY}`,
      },
    });

    if (testResponse.ok) {
      console.log("   ✅ Connection successful!");
    } else {
      const text = await testResponse.text();
      console.error(`   ❌ Connection failed: ${testResponse.status} ${text.slice(0, 200)}`);
      process.exit(1);
    }
  } catch (err) {
    console.error(`   ❌ Connection error: ${err}`);
    process.exit(1);
  }

  // Unfortunately, Supabase's REST API doesn't support raw SQL execution.
  // The setup SQL must be run either:
  // 1. In the Supabase Dashboard SQL Editor (copy-paste supabase/setup.sql)
  // 2. Via supabase CLI: supabase db push
  // 3. Via psql directly using the connection string

  console.log("\n" + "=".repeat(50));
  console.log("📋 NEXT STEPS:");
  console.log("=".repeat(50));
  console.log("\nThe Supabase REST API doesn't support raw DDL. Run the schema using ONE of:");
  console.log("\n  Option 1 (Easiest): Supabase Dashboard SQL Editor");
  console.log("    1. Go to https://supabase.com/dashboard");
  console.log("    2. Select your project → SQL Editor");
  console.log("    3. Copy-paste the contents of supabase/setup.sql");
  console.log("    4. Click 'Run'");
  console.log("\n  Option 2: Supabase CLI");
  console.log("    npx supabase login");
  console.log("    npx supabase link --project-ref YOUR_PROJECT_REF");
  console.log("    npx supabase db push");
  console.log("\n  Option 3: Direct psql (connection string from Supabase Dashboard → Settings → Database)");
  console.log("    psql 'postgresql://postgres:PASSWORD@HOST:5432/postgres' -f supabase/setup.sql");
  console.log("\nAfter running the schema, seed the data:");
  console.log("    npx tsx data/seed-all.ts");
  console.log("\nThen trigger a dog sync:");
  console.log(`    curl -H "Authorization: Bearer ${process.env.CRON_SECRET}" ${SUPABASE_URL?.replace('.supabase.co', '')}/api/cron/sync-dogs`);
}

main().catch(console.error);
