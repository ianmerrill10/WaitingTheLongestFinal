import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { rateLimit } from "@/lib/utils/rate-limit";

const PAGE_SIZE = 24;

export async function GET(request: Request) {
  const forwarded = request.headers.get("x-forwarded-for");
  const ip = forwarded?.split(",")[0]?.trim() || "unknown";
  const { allowed } = rateLimit(ip, 120, 60000);
  if (!allowed) {
    return NextResponse.json(
      { error: "Too many requests. Please try again later." },
      { status: 429, headers: { "Retry-After": "60", "X-RateLimit-Remaining": "0" } }
    );
  }

  const { searchParams } = new URL(request.url);

  const isExport = searchParams.get("export") === "true";
  const maxLimit = isExport ? 10000 : 200;
  const page = Math.max(1, parseInt(searchParams.get("page") || "1") || 1);
  const limit = Math.min(Math.max(1, parseInt(searchParams.get("limit") || String(PAGE_SIZE)) || PAGE_SIZE), maxLimit);
  const offset = (page - 1) * limit;
  const sort = searchParams.get("sort") || "name_asc";
  const search = searchParams.get("q");
  const city = searchParams.get("city");
  const zip = searchParams.get("zip");
  const county = searchParams.get("county");
  const state = searchParams.get("state");
  const type = searchParams.get("type");
  const killShelter = searchParams.get("kill_shelter");
  const rescuePulls = searchParams.get("rescue_pulls");
  const verified = searchParams.get("verified");
  const hasDogs = searchParams.get("has_dogs");
  const platform = searchParams.get("platform");

  const supabase = createAdminClient();

  let query = supabase
    .from("shelters")
    .select("*", { count: "exact" });

  // Escape SQL LIKE wildcards in user input
  const escapeLike = (s: string) => s.replace(/[%_\\]/g, "\\$&");

  // Text search filters
  if (search) query = query.ilike("name", `%${escapeLike(search)}%`);
  if (city) query = query.ilike("city", `%${escapeLike(city)}%`);
  if (zip) query = query.eq("zip_code", zip);
  if (county) query = query.ilike("county", `%${escapeLike(county)}%`);

  // Exact filters
  if (state) query = query.eq("state_code", state.toUpperCase());
  if (type) query = query.eq("shelter_type", type);
  if (platform) query = query.eq("website_platform", platform);

  // Boolean filters
  if (killShelter === "true") query = query.eq("is_kill_shelter", true);
  if (killShelter === "false") query = query.eq("is_kill_shelter", false);
  if (rescuePulls === "true") query = query.eq("accepts_rescue_pulls", true);
  if (rescuePulls === "false") query = query.eq("accepts_rescue_pulls", false);
  if (verified === "true") query = query.eq("is_verified", true);
  if (verified === "false") query = query.eq("is_verified", false);

  // Has dogs filter
  if (hasDogs === "true") query = query.gt("available_dog_count", 0);
  if (hasDogs === "false") query = query.eq("available_dog_count", 0);

  // Sort
  switch (sort) {
    case "name_desc":
      query = query.order("name", { ascending: false });
      break;
    case "state":
      query = query.order("state_code", { ascending: true }).order("name", { ascending: true });
      break;
    case "most_dogs":
      query = query.order("available_dog_count", { ascending: false }).order("name", { ascending: true });
      break;
    case "most_urgent":
      query = query.order("urgent_dog_count", { ascending: false }).order("available_dog_count", { ascending: false });
      break;
    case "recently_updated":
      query = query.order("updated_at", { ascending: false });
      break;
    case "recently_scraped":
      query = query.order("last_scraped_at", { ascending: false, nullsFirst: false });
      break;
    case "name_asc":
    default:
      query = query.order("name", { ascending: true });
      break;
  }

  // Paginate
  query = query.range(offset, offset + limit - 1);

  const { data, error, count } = await query;

  if (error) {
    console.error("[SheltersAPI] Query error:", error.message);
    return NextResponse.json({ error: "Failed to fetch shelters" }, { status: 500 });
  }

  return NextResponse.json({
    shelters: data || [],
    total: count || 0,
    page,
    limit,
    totalPages: Math.ceil((count || 0) / limit),
  });
}
