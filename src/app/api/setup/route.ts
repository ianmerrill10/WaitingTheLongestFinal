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
    const client = new Client({
      host: "db.hpssqzqwtsczsxvdfktt.supabase.co",
      port: 5432,
      user: "postgres",
      password: process.env.SUPABASE_DB_PASSWORD || "WtL_Sup4b@se2026!",
      database: "postgres",
      ssl: { rejectUnauthorized: false },
    });

    await client.connect();
    results.push("Connected to database");

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
