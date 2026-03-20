import { NextResponse } from "next/server";
import { Client } from "pg";

export const maxDuration = 30;

export async function GET(request: Request) {
  const authHeader = request.headers.get("authorization");
  if (authHeader !== `Bearer ${process.env.CRON_SECRET}`) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const results: string[] = [];

  try {
    // Try multiple connection methods
    const connectionConfigs = [
      {
        name: "session_pooler",
        host: "aws-0-us-east-1.pooler.supabase.com",
        port: 5432,
        user: "postgres.hpssqzqwtsczsxvdfktt",
        password: "WtL_Sup4b@se2026!",
        database: "postgres",
        ssl: { rejectUnauthorized: false },
      },
      {
        name: "transaction_pooler",
        host: "aws-0-us-east-1.pooler.supabase.com",
        port: 6543,
        user: "postgres.hpssqzqwtsczsxvdfktt",
        password: "WtL_Sup4b@se2026!",
        database: "postgres",
        ssl: { rejectUnauthorized: false },
      },
      {
        name: "direct",
        host: "db.hpssqzqwtsczsxvdfktt.supabase.co",
        port: 5432,
        user: "postgres",
        password: "WtL_Sup4b@se2026!",
        database: "postgres",
        ssl: { rejectUnauthorized: false },
      },
    ];

    let client: Client | null = null;
    for (const config of connectionConfigs) {
      try {
        const c = new Client({
          host: config.host,
          port: config.port,
          user: config.user,
          password: config.password,
          database: config.database,
          ssl: config.ssl,
          connectionTimeoutMillis: 10000,
        });
        await c.connect();
        results.push(`Connected via ${config.name}`);
        client = c;
        break;
      } catch (err) {
        results.push(`${config.name} failed: ${err instanceof Error ? err.message : String(err)}`);
      }
    }

    if (!client) {
      return NextResponse.json({
        success: false,
        results,
        error: "Could not connect to database with any method",
        sql_to_run_manually: "See /api/debug?fix=run_migration for the SQL to paste into Supabase SQL Editor",
      });
    }

    // Migration 008: foster_applications
    await client.query(`
      CREATE TABLE IF NOT EXISTS foster_applications (
        id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
        first_name TEXT NOT NULL,
        last_name TEXT NOT NULL,
        email TEXT NOT NULL,
        zip_code TEXT NOT NULL,
        housing_type TEXT NOT NULL,
        dog_experience TEXT NOT NULL,
        preferred_size TEXT DEFAULT 'any',
        notes TEXT,
        status TEXT NOT NULL DEFAULT 'pending',
        created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
        updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
      );
    `);
    results.push("foster_applications table: OK");

    await client.query(
      "CREATE INDEX IF NOT EXISTS idx_foster_applications_status ON foster_applications(status);"
    );
    await client.query(
      "CREATE INDEX IF NOT EXISTS idx_foster_applications_created ON foster_applications(created_at DESC);"
    );
    await client.query(
      "CREATE INDEX IF NOT EXISTS idx_foster_applications_zip ON foster_applications(zip_code);"
    );
    results.push("foster_applications indexes: OK");

    // Migration 010: birth_date columns + daily_stats table
    await client.query("ALTER TABLE dogs ADD COLUMN IF NOT EXISTS birth_date TIMESTAMPTZ;");
    await client.query("ALTER TABLE dogs ADD COLUMN IF NOT EXISTS is_birth_date_exact BOOLEAN DEFAULT NULL;");
    await client.query("ALTER TABLE dogs ADD COLUMN IF NOT EXISTS is_courtesy_listing BOOLEAN DEFAULT FALSE;");
    await client.query("CREATE INDEX IF NOT EXISTS idx_dogs_birth_date ON dogs(birth_date) WHERE birth_date IS NOT NULL;");
    await client.query("ALTER TABLE dogs ADD COLUMN IF NOT EXISTS external_url_alive BOOLEAN DEFAULT NULL;");
    results.push("dogs birth_date + external_url_alive columns: OK");

    await client.query(`
      CREATE TABLE IF NOT EXISTS daily_stats (
        id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
        stat_date DATE NOT NULL UNIQUE,
        total_available_dogs INTEGER NOT NULL DEFAULT 0,
        high_confidence_dogs INTEGER NOT NULL DEFAULT 0,
        avg_wait_days_all DECIMAL(10,2),
        avg_wait_days_high_confidence DECIMAL(10,2),
        median_wait_days DECIMAL(10,2),
        max_wait_days INTEGER,
        dogs_with_birth_date INTEGER DEFAULT 0,
        verification_coverage_pct DECIMAL(5,2),
        date_accuracy_pct DECIMAL(5,2),
        dogs_added_today INTEGER DEFAULT 0,
        dogs_adopted_today INTEGER DEFAULT 0,
        created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
      );
    `);
    await client.query("CREATE INDEX IF NOT EXISTS idx_daily_stats_date ON daily_stats(stat_date DESC);");
    results.push("daily_stats table: OK");

    // Enable RLS on daily_stats
    await client.query("ALTER TABLE daily_stats ENABLE ROW LEVEL SECURITY;");
    await client.query("CREATE POLICY IF NOT EXISTS daily_stats_read ON daily_stats FOR SELECT USING (true);");
    await client.query("CREATE POLICY IF NOT EXISTS daily_stats_write ON daily_stats FOR ALL USING (true) WITH CHECK (true);");
    results.push("daily_stats RLS: OK");

    await client.end();
    results.push("Done");

    return NextResponse.json({ success: true, results });
  } catch (error) {
    const message =
      error instanceof Error ? error.message : "Unknown error";
    return NextResponse.json(
      { success: false, results, error: message },
      { status: 500 }
    );
  }
}
