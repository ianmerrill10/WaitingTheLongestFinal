import { SupabaseClient } from '@supabase/supabase-js';
import { DOGS_PER_PAGE } from "@/lib/constants";

export async function getDogs(supabase: SupabaseClient, searchParams: URLSearchParams) {

  const page = Math.max(1, parseInt(searchParams.get("page") || "1") || 1);
  const limit = Math.min(Math.max(1, parseInt(searchParams.get("limit") || String(DOGS_PER_PAGE)) || DOGS_PER_PAGE), 100);
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

  let query = supabase
    .from("dogs")
    .select(
      `
      *,
      shelters (
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
  if (breed) {
    const sanitizedBreed = breed.replace(/[%_\(),.*]/g, "").substring(0, 100);
    if (sanitizedBreed) query = query.ilike("breed_primary", `%${sanitizedBreed}%`);
  }
  if (size) query = query.eq("size", size);
  if (age) query = query.eq("age_category", age);
  if (gender) query = query.eq("gender", gender);
  if (state) {
    const stateCode = state.toUpperCase();
    query = query.or(`state_code.eq.${stateCode},shelters.state_code.eq.${stateCode}`);
  }
  if (urgency) query = query.eq("urgency_level", urgency);
  if (search) {
    const sanitized = search.replace(/[%_\(),.*]/g, "").substring(0, 100);
    if (sanitized.length > 0) {
      query = query.or(`name.ilike.%${sanitized}%,breed_primary.ilike.%${sanitized}%`);
    }
  }

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
        const today = new Date();
        today.setHours(0, 0, 0, 0);
        query = query.gte("created_at", today.toISOString());
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
      query = query.order("created_at", { ascending: new Date().getDate() % 2 === 0 })
        .order("id", { ascending: new Date().getDate() % 3 !== 0 });
      break;
    case "wait_time":
    default:
      query = query.order("intake_date", { ascending: true });
      break;
  }

  // Paginate
  query = query.range(offset, offset + limit - 1);

  return query;
}
