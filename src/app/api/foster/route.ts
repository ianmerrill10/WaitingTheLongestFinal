import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";

const EMAIL_REGEX = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
const ZIP_REGEX = /^\d{5}(-\d{4})?$/;

const VALID_HOUSING = [
  "House with yard",
  "House without yard",
  "Apartment/Condo",
  "Townhouse",
  "Other",
];

const VALID_EXPERIENCE = [
  "First-time dog owner",
  "Some experience",
  "Experienced dog owner",
  "Professional / trainer",
];

const VALID_SIZES = [
  "any",
  "Small (under 25 lbs)",
  "Medium (25-50 lbs)",
  "Large (50-90 lbs)",
  "Extra Large (90+ lbs)",
];

export async function POST(request: Request) {
  let body;
  try {
    body = await request.json();
  } catch {
    return NextResponse.json(
      { error: "Invalid request body" },
      { status: 400 }
    );
  }

  const { firstName, lastName, email, zipCode, housingType, dogExperience, preferredSize, notes } = body;

  // Validate required fields
  if (!firstName || typeof firstName !== "string" || firstName.trim().length < 1) {
    return NextResponse.json({ error: "First name is required" }, { status: 400 });
  }
  if (!lastName || typeof lastName !== "string" || lastName.trim().length < 1) {
    return NextResponse.json({ error: "Last name is required" }, { status: 400 });
  }
  if (!email || typeof email !== "string" || !EMAIL_REGEX.test(email)) {
    return NextResponse.json({ error: "A valid email address is required" }, { status: 400 });
  }
  if (!zipCode || typeof zipCode !== "string" || !ZIP_REGEX.test(zipCode.trim())) {
    return NextResponse.json({ error: "A valid US zip code is required" }, { status: 400 });
  }
  if (!housingType || !VALID_HOUSING.includes(housingType)) {
    return NextResponse.json({ error: "Please select a housing type" }, { status: 400 });
  }
  if (!dogExperience || !VALID_EXPERIENCE.includes(dogExperience)) {
    return NextResponse.json({ error: "Please select your dog experience level" }, { status: 400 });
  }

  const size = preferredSize && VALID_SIZES.includes(preferredSize) ? preferredSize : "any";
  const sanitizedNotes = notes ? String(notes).slice(0, 2000).trim() : null;

  const supabase = createAdminClient();

  const { data, error } = await supabase
    .from("foster_applications")
    .insert({
      first_name: firstName.trim().slice(0, 100),
      last_name: lastName.trim().slice(0, 100),
      email: email.trim().toLowerCase().slice(0, 255),
      zip_code: zipCode.trim(),
      housing_type: housingType,
      dog_experience: dogExperience,
      preferred_size: size,
      notes: sanitizedNotes,
      status: "pending",
    })
    .select("id, created_at")
    .single();

  if (error) {
    console.error("Foster application error:", error);
    return NextResponse.json(
      { error: "Failed to submit application. Please try again." },
      { status: 500 }
    );
  }

  return NextResponse.json({
    success: true,
    id: data.id,
    message: "Thank you for applying to foster! We'll match you with a local shelter in your area and be in touch soon.",
  });
}

export async function GET(request: Request) {
  const authHeader = request.headers.get("authorization");
  const cronSecret = process.env.CRON_SECRET;
  if (!cronSecret || authHeader !== `Bearer ${cronSecret}`) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const supabase = createAdminClient();
  const { count, error } = await supabase
    .from("foster_applications")
    .select("*", { count: "exact", head: true });

  if (error) {
    return NextResponse.json({ error: error.message }, { status: 500 });
  }

  const { data: recent } = await supabase
    .from("foster_applications")
    .select("id, first_name, last_name, zip_code, status, created_at")
    .order("created_at", { ascending: false })
    .limit(10);

  return NextResponse.json({
    total_applications: count,
    recent: recent || [],
  });
}
