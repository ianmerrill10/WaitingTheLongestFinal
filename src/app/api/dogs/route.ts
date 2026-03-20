import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { DOGS_PER_PAGE } from "@/lib/constants";
import { rateLimit } from "@/lib/utils/rate-limit";

export async function GET(request: Request) {
  const forwarded = request.headers.get("x-forwarded-for");
  const ip = forwarded?.split(",")[0]?.trim() || "unknown";
  const { allowed, remaining } = rateLimit(ip, 120, 60000);
  if (!allowed) {
    return NextResponse.json(
      { error: "Too many requests. Please try again later." },
      { status: 429, headers: { "Retry-After": "60", "X-RateLimit-Remaining": "0" } }
    );
  }

  const { searchParams } = new URL(request.url);

  const page = parseInt(searchParams.get("page") || "1");
  const limit = Math.min(parseInt(searchParams.get("limit") || String(DOGS_PER_PAGE)), 100);
  const offset = (page - 1) * limit;
  const sort = searchParams.get("sort") || "wait_time";
  const breed = searchParams.get("breed");
  const size = searchParams.get("size");
  const age = searchParams.get("age");
  const gender = searchParams.get("gender");
  const state = searchParams.get("state");
  const urgency = searchParams.get("urgency");
  const search = searchParams.get("q");
  const shelterId = searchParams.get("shelter");

  const supabase = createAdminClient();

  let query = supabase
    .from("dogs")
    .select(
      `
      *,
      shelters!inner (
        name,
        city,
        state_code
      )
    `,
      { count: "exact" }
    )
    .eq("is_available", true);

  // Apply filters
  if (shelterId) query = query.eq("shelter_id", shelterId);
  if (breed) query = query.ilike("breed_primary", `%${breed}%`);
  if (size) query = query.eq("size", size);
  if (age) query = query.eq("age_category", age);
  if (gender) query = query.eq("gender", gender);
  if (state) query = query.eq("shelters.state_code", state.toUpperCase());
  if (urgency) query = query.eq("urgency_level", urgency);
  if (search) query = query.or(`name.ilike.%${search}%,breed_primary.ilike.%${search}%`);

  // Compatibility & discovery filters
  const goodWithKids = searchParams.get("good_with_kids");
  const goodWithDogs = searchParams.get("good_with_dogs");
  const goodWithCats = searchParams.get("good_with_cats");
  const houseTrained = searchParams.get("house_trained");
  const newToday = searchParams.get("new_today");
  const feeWaived = searchParams.get("fee_waived");

  if (goodWithKids === "true") query = query.eq("good_with_kids", true);
  if (goodWithDogs === "true") query = query.eq("good_with_dogs", true);
  if (goodWithCats === "true") query = query.eq("good_with_cats", true);
  if (houseTrained === "true") query = query.eq("house_trained", true);
  if (newToday === "true") {
    const yesterday = new Date(Date.now() - 86400000).toISOString();
    query = query.gte("created_at", yesterday);
  }
  if (feeWaived === "true") query = query.eq("is_fee_waived", true);

  // Apply sort
  switch (sort) {
    case "urgency":
      query = query.order("urgency_level", { ascending: true }).order("euthanasia_date", { ascending: true, nullsFirst: false });
      break;
    case "newest":
      query = query.order("created_at", { ascending: false });
      break;
    case "name":
      query = query.order("name", { ascending: true });
      break;
    case "random":
      // Use a daily seed so random order is consistent within a day but changes daily
      query = query.order("id", { ascending: new Date().getDate() % 2 === 0 }).limit(limit);
      break;
    case "wait_time":
    default:
      query = query.order("intake_date", { ascending: true });
      break;
  }

  // Paginate
  query = query.range(offset, offset + limit - 1);

  const { data, error, count } = await query;

  if (error) {
    return NextResponse.json({ error: error.message }, { status: 500 });
  }

  return NextResponse.json({
    dogs: data || [],
    total: count || 0,
    page,
    limit,
    totalPages: Math.ceil((count || 0) / limit),
  });
}
