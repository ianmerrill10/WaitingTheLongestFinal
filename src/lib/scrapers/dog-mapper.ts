/**
 * Universal dog mapper — normalizes scraped dog data to our DB schema.
 * Reuses patterns from src/lib/rescuegroups/mapper.ts.
 */
import type { ScrapedDog } from "./types";
import { parseDateFromDescription } from "@/lib/utils/parse-description-date";
import { parseUrgencyFromDescription } from "@/lib/utils/parse-urgency-from-description";

type DateConfidence = "verified" | "high" | "medium" | "low" | "unknown";

const SIZE_MAP: Record<string, string> = {
  small: "small",
  sm: "small",
  s: "small",
  toy: "small",
  mini: "small",
  miniature: "small",
  medium: "medium",
  med: "medium",
  m: "medium",
  large: "large",
  lg: "large",
  l: "large",
  "extra large": "xlarge",
  xlarge: "xlarge",
  xl: "xlarge",
  "x-large": "xlarge",
  giant: "xlarge",
};

function normalizeSize(
  sizeText?: string,
  weightLbs?: number
): "small" | "medium" | "large" | "xlarge" {
  if (sizeText) {
    const mapped = SIZE_MAP[sizeText.toLowerCase().trim()];
    if (mapped) return mapped as "small" | "medium" | "large" | "xlarge";
  }
  if (weightLbs) {
    if (weightLbs < 25) return "small";
    if (weightLbs < 50) return "medium";
    if (weightLbs < 90) return "large";
    return "xlarge";
  }
  return "medium";
}

function normalizeGender(text?: string): "male" | "female" {
  if (!text) return "male";
  const lower = text.toLowerCase().trim();
  if (
    lower === "female" ||
    lower === "f" ||
    lower === "girl" ||
    lower === "spayed female"
  )
    return "female";
  return "male";
}

/**
 * Parse age text to months.
 * Handles: "3 years", "2 Years 5 Months", "6 months", "8 weeks", "1 yr", etc.
 */
export function parseAgeToMonths(ageText?: string): number | null {
  if (!ageText) return null;
  const text = ageText.toLowerCase().trim();

  let months = 0;
  let found = false;

  // Years
  const yearMatch = text.match(/(\d+)\s*(?:year|yr|y)\w*/);
  if (yearMatch) {
    months += parseInt(yearMatch[1]) * 12;
    found = true;
  }

  // Months
  const monthMatch = text.match(/(\d+)\s*(?:month|mo|m)\w*/);
  if (monthMatch) {
    months += parseInt(monthMatch[1]);
    found = true;
  }

  // Weeks
  const weekMatch = text.match(/(\d+)\s*(?:week|wk|w)\w*/);
  if (weekMatch) {
    months += Math.round(parseInt(weekMatch[1]) / 4.33);
    found = true;
  }

  // Days
  const dayMatch = text.match(/(\d+)\s*(?:day|d)\w*/);
  if (dayMatch) {
    months += Math.round(parseInt(dayMatch[1]) / 30);
    found = true;
  }

  return found ? months : null;
}

function computeAgeCategory(
  ageMonths: number | null
): "puppy" | "young" | "adult" | "senior" {
  if (ageMonths === null) return "adult";
  if (ageMonths < 12) return "puppy";
  if (ageMonths < 36) return "young";
  if (ageMonths < 84) return "adult";
  return "senior";
}

const MAX_WAIT_DAYS: Record<DateConfidence, number> = {
  verified: 4380, // 12 years
  high: 4380,
  medium: 2555, // 7 years
  low: 730, // 2 years
  unknown: 365, // 1 year
};

/**
 * Map a scraped dog to our database schema.
 */
export function mapScrapedDog(
  dog: ScrapedDog,
  shelterId: string,
  platform: string
): Record<string, unknown> {
  const ageMonths = dog.age_months ?? parseAgeToMonths(dog.age_text);
  const size = normalizeSize(dog.size, dog.weight_lbs);
  const gender = normalizeGender(dog.gender);
  const ageCategory = computeAgeCategory(ageMonths);

  // Compute intake date with confidence
  let intake_date: string;
  let date_confidence: DateConfidence;
  let date_source: string;

  if (dog.intake_date) {
    // Shelter-provided date from the listing page — validate before using
    const parsedIntake = new Date(dog.intake_date);
    if (!isNaN(parsedIntake.getTime())) {
      intake_date = parsedIntake.toISOString();
      date_confidence = "verified";
      date_source = `scraper_${platform}_listing_date`;
    } else {
      // Invalid date string — fall through to description parsing or default
      intake_date = new Date().toISOString();
      date_confidence = "low";
      date_source = `scraper_${platform}_invalid_date`;
    }
  } else if (dog.description) {
    // Try to parse date from description text
    const parsed = parseDateFromDescription(dog.description);
    if (parsed) {
      intake_date = parsed.date.toISOString();
      date_confidence = parsed.confidence as DateConfidence;
      date_source = `scraper_${platform}_description_parsed`;
    } else {
      intake_date = new Date().toISOString();
      date_confidence = "low";
      date_source = `scraper_${platform}_no_date`;
    }
  } else {
    intake_date = new Date().toISOString();
    date_confidence = "low";
    date_source = `scraper_${platform}_no_date`;
  }

  // Apply max wait time cap
  const maxDays = MAX_WAIT_DAYS[date_confidence];
  const intakeMs = new Date(intake_date).getTime();
  const cutoffMs = Date.now() - maxDays * 86400000;
  if (intakeMs < cutoffMs) {
    intake_date = new Date(cutoffMs).toISOString();
    date_confidence = date_confidence === "verified" ? "medium" : "low";
    date_source += `|wait_capped_${maxDays}d`;
  }

  // Check for urgency — prefer explicit euthanasia_date from scraper, then description parsing
  let urgency_level = "normal";
  let euthanasia_date: string | null = null;
  let is_on_euthanasia_list = dog.is_on_euthanasia_list ?? false;

  // Priority 1: Explicit euthanasia_date from scraper adapter
  if (dog.euthanasia_date) {
    const euthDate = new Date(dog.euthanasia_date);
    if (!isNaN(euthDate.getTime()) && euthDate > new Date()) {
      euthanasia_date = euthDate.toISOString();
      is_on_euthanasia_list = true;
      const hoursLeft = (euthDate.getTime() - Date.now()) / (1000 * 60 * 60);
      if (hoursLeft > 0 && hoursLeft < 24) urgency_level = "critical";
      else if (hoursLeft > 0 && hoursLeft < 72) urgency_level = "high";
      else if (hoursLeft > 0 && hoursLeft < 168) urgency_level = "medium";
    }
  }

  // Priority 2: Parse urgency from description text
  if (!euthanasia_date && dog.description) {
    const urgency = parseUrgencyFromDescription(dog.description);
    if (urgency && urgency.date && urgency.confidence === "high") {
      euthanasia_date = urgency.date.toISOString();
      is_on_euthanasia_list = true;
      const hoursLeft =
        (urgency.date.getTime() - Date.now()) / (1000 * 60 * 60);
      if (hoursLeft > 0 && hoursLeft < 24) urgency_level = "critical";
      else if (hoursLeft > 0 && hoursLeft < 72) urgency_level = "high";
      else if (hoursLeft > 0 && hoursLeft < 168) urgency_level = "medium";
    }
  }

  return {
    name: (dog.name || "Unknown").substring(0, 200),
    shelter_id: shelterId,
    breed_primary: (dog.breed_primary || "Mixed Breed").substring(0, 100),
    breed_secondary: dog.breed_secondary?.substring(0, 100) || null,
    breed_mixed: dog.breed_mixed ?? false,
    size,
    age_category: ageCategory,
    age_months: ageMonths,
    gender,
    color_primary: dog.color_primary?.substring(0, 50) || null,
    color_secondary: dog.color_secondary?.substring(0, 50) || null,
    weight_lbs: dog.weight_lbs || null,
    is_spayed_neutered: dog.is_spayed_neutered ?? false,
    status: "available",
    is_available: true,
    intake_date,
    date_confidence,
    date_source,
    intake_type: "scraper",
    euthanasia_date,
    urgency_level,
    is_on_euthanasia_list,
    photo_urls: dog.photo_urls || [],
    primary_photo_url: dog.primary_photo_url || dog.photo_urls?.[0] || null,
    thumbnail_url: dog.primary_photo_url || dog.photo_urls?.[0] || null,
    description: dog.description?.substring(0, 5000) || null,
    tags: dog.tags || [],
    good_with_kids: dog.good_with_kids ?? null,
    good_with_dogs: dog.good_with_dogs ?? null,
    good_with_cats: dog.good_with_cats ?? null,
    house_trained: dog.house_trained ?? null,
    has_special_needs: dog.has_special_needs ?? false,
    special_needs_description:
      dog.special_needs_description?.substring(0, 1000) || null,
    adoption_fee: dog.adoption_fee ?? null,
    external_id: dog.external_id,
    external_source: `scraper_${platform}`,
    external_url: dog.external_url?.substring(0, 500) || null,
    verification_status: "verified",
    last_verified_at: new Date().toISOString(),
  };
}
