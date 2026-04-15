import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { rateLimit } from "@/lib/utils/rate-limit";
import { logger } from "@/lib/utils/logger";

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
  try {
    // Rate limit: 5 foster applications per minute per IP
    const forwarded = request.headers.get("x-forwarded-for");
    const ip = forwarded?.split(",")[0]?.trim() || "unknown";
    const { allowed } = rateLimit(`foster:${ip}`, 5, 60000);
    if (!allowed) {
      return NextResponse.json(
        { error: "Too many requests. Please try again later." },
        { status: 429, headers: { "Retry-After": "60" } }
      );
    }

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
      logger.error("foster_application_insert_failed", { error });
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
  } catch (err) {
    logger.error("foster_post_unhandled", { error: err });
    return NextResponse.json(
      { error: "An unexpected error occurred. Please try again." },
      { status: 500 }
    );
  }
}

export async function GET(request: Request) {
  try {
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
      logger.error("foster_get_count_failed", { error });
      return NextResponse.json({ error: "Failed to fetch applications" }, { status: 500 });
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
  } catch (err) {
    logger.error("foster_get_unhandled", { error: err });
    return NextResponse.json(
      { error: "An unexpected error occurred." },
      { status: 500 }
    );
  }
}
