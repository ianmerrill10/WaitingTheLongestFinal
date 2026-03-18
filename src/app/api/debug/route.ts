import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { RESCUEGROUPS_API_BASE } from "@/lib/constants";

export const runtime = "nodejs";
export const maxDuration = 30;

// Environment variables to check (never expose values)
const ENV_VARS_TO_CHECK = [
  "NEXT_PUBLIC_SUPABASE_URL",
  "NEXT_PUBLIC_SUPABASE_ANON_KEY",
  "SUPABASE_SERVICE_ROLE_KEY",
  "RESCUEGROUPS_API_KEY",
  "CRON_SECRET",
  "THE_DOG_API_KEY",
  "THE_DOG_API_KEY_2",
  "THE_DOG_API_KEY_3",
  "THE_DOG_API_KEY_4",
  "SENDGRID_API_KEY",
  "GOOGLE_CLIENT_ID",
  "GOOGLE_CLIENT_SECRET",
  "FACEBOOK_APP_ID",
  "FACEBOOK_APP_SECRET",
];

export async function GET(request: Request) {
  // Verify authorization
  const authHeader = request.headers.get("authorization");
  if (authHeader !== `Bearer ${process.env.CRON_SECRET}`) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const url = new URL(request.url);
  const fixParam = url.searchParams.get("fix");
  const startTime = Date.now();

  const dashboard: Record<string, unknown> = {
    timestamp: new Date().toISOString(),
    version: "1.0",
  };

  // --- Database connectivity ---
  let supabase;
  try {
    supabase = createAdminClient();
    const { data, error } = await supabase
      .from("states")
      .select("code")
      .limit(1);
    dashboard.database = {
      connected: !error,
      error: error ? error.message : null,
      latency_ms: Date.now() - startTime,
    };
  } catch (err) {
    dashboard.database = {
      connected: false,
      error: String(err),
      latency_ms: Date.now() - startTime,
    };
    // If DB is down, return early with what we have
    dashboard.environment = buildEnvCheck();
    dashboard.total_time_ms = Date.now() - startTime;
    return NextResponse.json(dashboard, { status: 500 });
  }

  // --- Dog counts ---
  try {
    const [totalResult, availableResult, unavailableResult] = await Promise.all([
      supabase.from("dogs").select("id", { count: "exact", head: true }),
      supabase
        .from("dogs")
        .select("id", { count: "exact", head: true })
        .eq("is_available", true),
      supabase
        .from("dogs")
        .select("id", { count: "exact", head: true })
        .eq("is_available", false),
    ]);

    dashboard.dogs = {
      total: totalResult.count ?? 0,
      available: availableResult.count ?? 0,
      unavailable: unavailableResult.count ?? 0,
      errors: [totalResult.error, availableResult.error, unavailableResult.error]
        .filter(Boolean)
        .map((e) => e!.message),
    };
  } catch (err) {
    dashboard.dogs = { error: String(err) };
  }

  // --- Dogs by urgency level ---
  try {
    const [criticalResult, highResult, mediumResult, normalResult] =
      await Promise.all([
        supabase
          .from("dogs")
          .select("id", { count: "exact", head: true })
          .eq("urgency_level", "critical"),
        supabase
          .from("dogs")
          .select("id", { count: "exact", head: true })
          .eq("urgency_level", "high"),
        supabase
          .from("dogs")
          .select("id", { count: "exact", head: true })
          .eq("urgency_level", "medium"),
        supabase
          .from("dogs")
          .select("id", { count: "exact", head: true })
          .eq("urgency_level", "normal"),
      ]);

    dashboard.dogs_by_urgency = {
      critical: criticalResult.count ?? 0,
      high: highResult.count ?? 0,
      medium: mediumResult.count ?? 0,
      normal: normalResult.count ?? 0,
    };
  } catch (err) {
    dashboard.dogs_by_urgency = { error: String(err) };
  }

  // --- Dogs with bad intake_date (older than 5 years) ---
  try {
    const fiveYearsAgo = new Date();
    fiveYearsAgo.setFullYear(fiveYearsAgo.getFullYear() - 5);

    const { data: badDateDogs, count, error } = await supabase
      .from("dogs")
      .select("id, name, intake_date, last_synced_at, external_id", {
        count: "exact",
      })
      .lt("intake_date", fiveYearsAgo.toISOString())
      .order("intake_date", { ascending: true })
      .limit(20);

    dashboard.bad_intake_dates = {
      count: count ?? 0,
      sample: badDateDogs?.map((d) => ({
        id: d.id,
        name: d.name,
        intake_date: d.intake_date,
        last_synced_at: d.last_synced_at,
        external_id: d.external_id,
      })) ?? [],
      error: error ? error.message : null,
    };
  } catch (err) {
    dashboard.bad_intake_dates = { error: String(err) };
  }

  // --- Dogs with no photos ---
  try {
    const { count, error } = await supabase
      .from("dogs")
      .select("id", { count: "exact", head: true })
      .is("primary_photo_url", null)
      .eq("is_available", true);

    dashboard.dogs_no_photos = {
      available_without_photo: count ?? 0,
      error: error ? error.message : null,
    };
  } catch (err) {
    dashboard.dogs_no_photos = { error: String(err) };
  }

  // --- Shelter counts ---
  try {
    const [totalResult, verifiedResult] = await Promise.all([
      supabase.from("shelters").select("id", { count: "exact", head: true }),
      supabase
        .from("shelters")
        .select("id", { count: "exact", head: true })
        .eq("is_verified", true),
    ]);

    // Top 10 states by shelter count using RPC or manual aggregation
    const { data: sheltersByState, error: stateError } = await supabase
      .from("shelters")
      .select("state_code");

    let top10States: { state: string; count: number }[] = [];
    if (sheltersByState && !stateError) {
      const stateCounts: Record<string, number> = {};
      for (const s of sheltersByState) {
        stateCounts[s.state_code] = (stateCounts[s.state_code] || 0) + 1;
      }
      top10States = Object.entries(stateCounts)
        .map(([state, count]) => ({ state, count }))
        .sort((a, b) => b.count - a.count)
        .slice(0, 10);
    }

    dashboard.shelters = {
      total: totalResult.count ?? 0,
      verified: verifiedResult.count ?? 0,
      top_10_states: top10States,
      errors: [totalResult.error, verifiedResult.error, stateError]
        .filter(Boolean)
        .map((e) => e!.message),
    };
  } catch (err) {
    dashboard.shelters = { error: String(err) };
  }

  // --- Last sync timestamp ---
  try {
    const { data, error } = await supabase
      .from("dogs")
      .select("last_synced_at")
      .not("last_synced_at", "is", null)
      .order("last_synced_at", { ascending: false })
      .limit(1)
      .single();

    dashboard.last_sync = {
      last_synced_at: data?.last_synced_at ?? null,
      error: error && error.code !== "PGRST116" ? error.message : null,
    };
  } catch (err) {
    dashboard.last_sync = { error: String(err) };
  }

  // --- Environment check ---
  dashboard.environment = buildEnvCheck();

  // --- RescueGroups API connectivity ---
  try {
    const apiKey = process.env.RESCUEGROUPS_API_KEY;
    if (!apiKey) {
      dashboard.rescuegroups_api = {
        status: "no_api_key",
        connected: false,
      };
    } else {
      const rgStart = Date.now();
      const response = await fetch(
        `${RESCUEGROUPS_API_BASE}/public/animals/search/available/dogs/?limit=1`,
        {
          method: "GET",
          headers: {
            Authorization: apiKey,
            "Content-Type": "application/vnd.api+json",
          },
          signal: AbortSignal.timeout(10000),
        }
      );

      dashboard.rescuegroups_api = {
        connected: response.ok,
        status_code: response.status,
        status_text: response.statusText,
        latency_ms: Date.now() - rgStart,
      };
    }
  } catch (err) {
    dashboard.rescuegroups_api = {
      connected: false,
      error: String(err),
    };
  }

  // --- Fix bad dates (optional repair tool) ---
  if (fixParam === "bad_dates") {
    try {
      const fiveYearsAgo = new Date();
      fiveYearsAgo.setFullYear(fiveYearsAgo.getFullYear() - 5);

      // Find all dogs with intake_date older than 5 years
      const { data: badDogs, error: fetchError } = await supabase
        .from("dogs")
        .select("id, intake_date, last_synced_at")
        .lt("intake_date", fiveYearsAgo.toISOString());

      if (fetchError) {
        dashboard.fix_bad_dates = {
          success: false,
          error: fetchError.message,
        };
      } else if (!badDogs || badDogs.length === 0) {
        dashboard.fix_bad_dates = {
          success: true,
          message: "No dogs with intake_date older than 5 years found",
          updated: 0,
        };
      } else {
        let updatedCount = 0;
        let errorCount = 0;
        const errors: string[] = [];

        // Update each dog's intake_date to their last_synced_at
        for (const dog of badDogs) {
          const newIntakeDate = dog.last_synced_at || new Date().toISOString();
          const { error: updateError } = await supabase
            .from("dogs")
            .update({ intake_date: newIntakeDate })
            .eq("id", dog.id);

          if (updateError) {
            errorCount++;
            if (errors.length < 5) {
              errors.push(`Dog ${dog.id}: ${updateError.message}`);
            }
          } else {
            updatedCount++;
          }
        }

        dashboard.fix_bad_dates = {
          success: errorCount === 0,
          found: badDogs.length,
          updated: updatedCount,
          errors: errorCount,
          sample_errors: errors.length > 0 ? errors : undefined,
        };
      }
    } catch (err) {
      dashboard.fix_bad_dates = {
        success: false,
        error: String(err),
      };
    }
  }

  dashboard.total_time_ms = Date.now() - startTime;

  return NextResponse.json(dashboard);
}

function buildEnvCheck(): Record<string, boolean> {
  const result: Record<string, boolean> = {};
  for (const key of ENV_VARS_TO_CHECK) {
    result[key] = !!process.env[key] && process.env[key]!.length > 0;
  }
  return result;
}
