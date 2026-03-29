import { NextRequest, NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";

// Auth enforced by middleware — /api/admin/* requires site_auth cookie
// Partner dashboard pages also validate wtl_shelter_id cookie matches

export async function GET(request: NextRequest) {
  try {
    const shelterId = request.nextUrl.searchParams.get("shelter_id");
    if (!shelterId) {
      return NextResponse.json(
        { error: "shelter_id required" },
        { status: 400 }
      );
    }

    // Validate UUID format to prevent injection
    const uuidRegex =
      /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
    if (!uuidRegex.test(shelterId)) {
      return NextResponse.json(
        { error: "Invalid shelter_id format" },
        { status: 400 }
      );
    }

    const supabase = createAdminClient();

    // Run all queries in parallel for performance
    const [
      shelterResult,
      totalDogsResult,
      availableDogsResult,
      adoptedDogsResult,
      urgentDogsResult,
      viewDataResult,
      keysResult,
      webhooksResult,
      communicationsResult,
      dogsResult,
      topDogsResult,
    ] = await Promise.all([
      supabase
        .from("shelters")
        .select("id, name, city, state_code, partner_status, partner_tier")
        .eq("id", shelterId)
        .single(),
      supabase
        .from("dogs")
        .select("*", { count: "exact", head: true })
        .eq("shelter_id", shelterId),
      supabase
        .from("dogs")
        .select("*", { count: "exact", head: true })
        .eq("shelter_id", shelterId)
        .eq("status", "available"),
      supabase
        .from("dogs")
        .select("*", { count: "exact", head: true })
        .eq("shelter_id", shelterId)
        .eq("status", "adopted"),
      supabase
        .from("dogs")
        .select("*", { count: "exact", head: true })
        .eq("shelter_id", shelterId)
        .in("urgency_level", ["critical", "high"]),
      supabase
        .from("dogs")
        .select("view_count")
        .eq("shelter_id", shelterId),
      supabase
        .from("shelter_api_keys")
        .select(
          "id, key_prefix, label, environment, scopes, is_active, last_used_at, usage_count_today, usage_count_total, created_at"
        )
        .eq("shelter_id", shelterId)
        .order("created_at", { ascending: false }),
      supabase
        .from("webhook_endpoints")
        .select(
          "id, url, events, is_active, total_deliveries, total_successes, total_failures, last_delivery_at"
        )
        .eq("shelter_id", shelterId)
        .order("created_at", { ascending: false }),
      supabase
        .from("shelter_communications")
        .select("id, comm_type, direction, subject, status, created_at")
        .eq("shelter_id", shelterId)
        .order("created_at", { ascending: false })
        .limit(50),
      supabase
        .from("dogs")
        .select("id, name, breed_primary, status, view_count, intake_date")
        .eq("shelter_id", shelterId)
        .order("intake_date", { ascending: false })
        .limit(100),
      supabase
        .from("dogs")
        .select("id, name, view_count, status")
        .eq("shelter_id", shelterId)
        .order("view_count", { ascending: false })
        .limit(10),
    ]);

    const totalViews = (viewDataResult.data || []).reduce(
      (sum, d) => sum + (d.view_count || 0),
      0
    );

    return NextResponse.json({
      shelter: shelterResult.data || null,
      stats: {
        total_dogs: totalDogsResult.count || 0,
        available_dogs: availableDogsResult.count || 0,
        adopted_dogs: adoptedDogsResult.count || 0,
        urgent_dogs: urgentDogsResult.count || 0,
        total_views: totalViews,
      },
      keys: (keysResult.data || []).map((k) => ({
        ...k,
        // Mask key prefix for security — only show first 10 chars
        key_prefix: k.key_prefix
          ? k.key_prefix.substring(0, 10) + "****"
          : "****",
      })),
      webhooks: webhooksResult.data || [],
      communications: communicationsResult.data || [],
      dogs: (dogsResult.data || []).map((d) => ({
        id: d.id,
        name: d.name,
        breed: d.breed_primary || "Unknown",
        status: d.status,
        views: d.view_count || 0,
        listed_date: d.intake_date || "",
      })),
      topDogs: (topDogsResult.data || []).map((d) => ({
        id: d.id,
        name: d.name,
        views: d.view_count || 0,
        status: d.status,
      })),
    });
  } catch (err) {
    console.error("Admin partner API error:", err);
    return NextResponse.json(
      { error: "Failed to load partner data" },
      { status: 500 }
    );
  }
}
