import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

/**
 * Public health endpoint — NO auth required.
 * Used by monitors (Uptime Robot, Pingdom, Vercel health checks).
 *
 * Returns 200 if the app can reach the database.
 * Returns 503 if the database is unreachable or paused.
 *
 * Response shape is intentionally minimal — no internal details leaked.
 */
export async function GET() {
  const startedAt = Date.now();

  try {
    const supabase = createAdminClient();
    const { error } = await supabase
      .from("dogs")
      .select("id", { count: "exact", head: true })
      .limit(1);

    const dbMs = Date.now() - startedAt;

    if (error) {
      return NextResponse.json(
        {
          status: "degraded",
          db: "error",
          uptime_s: Math.floor(process.uptime()),
        },
        { status: 503, headers: { "cache-control": "no-store" } }
      );
    }

    return NextResponse.json(
      {
        status: "ok",
        db: "ok",
        db_ms: dbMs,
        uptime_s: Math.floor(process.uptime()),
      },
      { status: 200, headers: { "cache-control": "no-store" } }
    );
  } catch {
    return NextResponse.json(
      {
        status: "degraded",
        db: "unreachable",
        uptime_s: Math.floor(process.uptime()),
      },
      { status: 503, headers: { "cache-control": "no-store" } }
    );
  }
}

export async function HEAD() {
  // HEAD requests return no body — useful for simple liveness pings
  return new NextResponse(null, { status: 200, headers: { "cache-control": "no-store" } });
}
