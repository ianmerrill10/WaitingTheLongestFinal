import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { DOGS_PER_PAGE } from "@/lib/constants";

export async function GET(request: Request) {
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
  if (breed) query = query.ilike("breed_primary", `%${breed}%`);
  if (size) query = query.eq("size", size);
  if (age) query = query.eq("age_category", age);
  if (gender) query = query.eq("gender", gender);
  if (state) query = query.eq("shelters.state_code", state.toUpperCase());
  if (urgency) query = query.eq("urgency_level", urgency);
  if (search) query = query.or(`name.ilike.%${search}%,breed_primary.ilike.%${search}%`);

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
