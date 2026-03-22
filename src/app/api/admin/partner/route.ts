import { NextRequest, NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";

export async function GET(request: NextRequest) {
  const shelterId = request.nextUrl.searchParams.get("shelter_id");
  if (!shelterId) {
    return NextResponse.json({ error: "shelter_id required" }, { status: 400 });
  }

  const supabase = createAdminClient();

  // Get shelter info
  const { data: shelter } = await supabase
    .from("shelters")
    .select("id, name, city, state, partner_status, partner_tier")
    .eq("id", shelterId)
    .single();

  // Get dog stats
  const { count: totalDogs } = await supabase
    .from("dogs")
    .select("*", { count: "exact", head: true })
    .eq("shelter_id", shelterId);

  const { count: availableDogs } = await supabase
    .from("dogs")
    .select("*", { count: "exact", head: true })
    .eq("shelter_id", shelterId)
    .eq("status", "available");

  const { count: adoptedDogs } = await supabase
    .from("dogs")
    .select("*", { count: "exact", head: true })
    .eq("shelter_id", shelterId)
    .eq("status", "adopted");

  const { count: urgentDogs } = await supabase
    .from("dogs")
    .select("*", { count: "exact", head: true })
    .eq("shelter_id", shelterId)
    .in("urgency_level", ["critical", "high"]);

  // Get total views
  const { data: viewData } = await supabase
    .from("dogs")
    .select("view_count")
    .eq("shelter_id", shelterId);

  const totalViews = (viewData || []).reduce(
    (sum, d) => sum + (d.view_count || 0),
    0
  );

  // Get API keys
  const { data: keys } = await supabase
    .from("shelter_api_keys")
    .select("id, key_prefix, label, environment, scopes, is_active, last_used_at, usage_count_today, usage_count_total, created_at")
    .eq("shelter_id", shelterId)
    .order("created_at", { ascending: false });

  // Get webhook endpoints
  const { data: webhooks } = await supabase
    .from("webhook_endpoints")
    .select("id, url, events, is_active, total_deliveries, total_successes, total_failures, last_delivery_at")
    .eq("shelter_id", shelterId)
    .order("created_at", { ascending: false });

  // Get communications
  const { data: communications } = await supabase
    .from("shelter_communications")
    .select("id, comm_type, direction, subject, status, created_at")
    .eq("shelter_id", shelterId)
    .order("created_at", { ascending: false })
    .limit(50);

  // Get dogs for management view
  const { data: dogs } = await supabase
    .from("dogs")
    .select("id, name, breed_primary, status, view_count, intake_date")
    .eq("shelter_id", shelterId)
    .order("intake_date", { ascending: false })
    .limit(100);

  // Get top viewed dogs for analytics
  const { data: topDogs } = await supabase
    .from("dogs")
    .select("id, name, view_count, status")
    .eq("shelter_id", shelterId)
    .order("view_count", { ascending: false })
    .limit(10);

  return NextResponse.json({
    shelter: shelter || null,
    stats: {
      total_dogs: totalDogs || 0,
      available_dogs: availableDogs || 0,
      adopted_dogs: adoptedDogs || 0,
      urgent_dogs: urgentDogs || 0,
      total_views: totalViews,
    },
    keys: keys || [],
    webhooks: webhooks || [],
    communications: communications || [],
    dogs: (dogs || []).map((d) => ({
      id: d.id,
      name: d.name,
      breed: d.breed_primary || "Unknown",
      status: d.status,
      views: d.view_count || 0,
      listed_date: d.intake_date || "",
    })),
    topDogs: (topDogs || []).map((d) => ({
      id: d.id,
      name: d.name,
      views: d.view_count || 0,
      status: d.status,
    })),
  });
}
