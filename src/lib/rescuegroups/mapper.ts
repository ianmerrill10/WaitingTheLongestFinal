// Maps RescueGroups v5 API data to our dog schema
// Based on real API response structure

import type { RGAnimalAttributes, RGIncludedItem } from "./client";
import { parseDateFromDescription } from "@/lib/utils/parse-description-date";

export type DateConfidence = "verified" | "high" | "medium" | "low" | "unknown";

export interface MappedDog {
  name: string;
  breed_primary: string;
  breed_secondary: string | null;
  breed_mixed: boolean;
  size: "small" | "medium" | "large" | "xlarge";
  age_category: "puppy" | "young" | "adult" | "senior";
  age_months: number | null;
  gender: "male" | "female";
  color_primary: string | null;
  description: string | null;
  primary_photo_url: string | null;
  photo_urls: string[];
  thumbnail_url: string | null;
  status: string;
  is_spayed_neutered: boolean;
  house_trained: boolean | null;
  has_special_needs: boolean;
  good_with_kids: boolean | null;
  good_with_dogs: boolean | null;
  good_with_cats: boolean | null;
  energy_level: "low" | "medium" | "high" | null;
  tags: string[];
  intake_date: string;
  date_confidence: DateConfidence;
  date_source: string;
  birth_date: string | null;
  is_birth_date_exact: boolean | null;
  is_courtesy_listing: boolean;
  external_id: string;
  external_source: string;
  external_url: string | null;
  last_synced_at: string;
}

const SIZE_MAP: Record<string, "small" | "medium" | "large" | "xlarge"> = {
  Small: "small",
  Medium: "medium",
  Large: "large",
  "X-Large": "xlarge",
  "Extra Large": "xlarge",
};

const AGE_MAP: Record<string, "puppy" | "young" | "adult" | "senior"> = {
  Baby: "puppy",
  Young: "young",
  Adult: "adult",
  Senior: "senior",
};

const GENDER_MAP: Record<string, "male" | "female"> = {
  Male: "male",
  Female: "female",
};

const ENERGY_MAP: Record<string, "low" | "medium" | "high"> = {
  Low: "low",
  "Slightly Active": "low",
  Moderate: "medium",
  "Moderately Active": "medium",
  High: "high",
  "Highly Active": "high",
};

/**
 * Extract photo URLs from the included pictures in the API response.
 */
export function extractPhotoUrls(
  pictureIds: string[],
  included: RGIncludedItem[]
): { photos: string[]; primary: string | null; thumbnail: string | null } {
  const photos: string[] = [];
  let primary: string | null = null;
  let thumbnail: string | null = null;

  for (const id of pictureIds) {
    const pic = included.find((i) => i.type === "pictures" && i.id === id);
    if (!pic) continue;

    const attrs = pic.attributes as {
      original?: { url: string };
      large?: { url: string };
      small?: { url: string };
      order?: number;
    };

    const url = attrs.large?.url || attrs.original?.url;
    if (url) photos.push(url);

    if (attrs.order === 1) {
      primary = attrs.large?.url || attrs.original?.url || null;
      thumbnail = attrs.small?.url || null;
    }
  }

  return { photos, primary, thumbnail };
}

/**
 * Parse age in months from RG ageString (e.g. "1 Year 7 Months", "3 Months")
 * or from description text (e.g. "Age: 1 year and 7 months").
 */
export function parseAgeMonths(ageString?: string | null, description?: string | null): number | null {
  // Try ageString from RG API first
  const parsed = parseAgeText(ageString);
  if (parsed !== null) return parsed;

  // Try extracting age from description
  if (description) {
    // Pattern: "Age: 1 year and 7 months" or "Age: 3 months" or "age is 2 years old"
    const agePatterns = [
      /\bage\s*(?:is|:)?\s*(\d+\s*years?\s*(?:and\s*)?\d*\s*months?|\d+\s*months?|\d+\s*years?\s*old|\d+\s*years?)/i,
      /\b(\d+)\s*years?\s*(?:and\s*)?(\d+)\s*months?\s*old\b/i,
    ];

    for (const pattern of agePatterns) {
      const match = pattern.exec(description);
      if (match) {
        const result = parseAgeText(match[0]);
        if (result !== null) return result;
      }
    }
  }

  return null;
}

function parseAgeText(text?: string | null): number | null {
  if (!text) return null;

  let totalMonths = 0;
  let found = false;

  // Match years
  const yearMatch = /(\d+)\s*years?/i.exec(text);
  if (yearMatch) {
    totalMonths += parseInt(yearMatch[1]) * 12;
    found = true;
  }

  // Match months
  const monthMatch = /(\d+)\s*months?/i.exec(text);
  if (monthMatch) {
    totalMonths += parseInt(monthMatch[1]);
    found = true;
  }

  // Match weeks (convert to months roughly)
  const weekMatch = /(\d+)\s*weeks?/i.exec(text);
  if (weekMatch) {
    totalMonths += Math.max(1, Math.round(parseInt(weekMatch[1]) / 4));
    found = true;
  }

  return found && totalMonths > 0 ? totalMonths : null;
}

/**
 * Compute age_months using best available source.
 * Priority: birthDate from API > ageString text > description text.
 */
function computeAgeMonthsFromSources(
  attrs: RGAnimalAttributes,
  description: string | null,
): { ageMonths: number | null; birthDateStr: string | null } {
  // Strategy 1: birthDate from RG API (gold standard)
  if (attrs.birthDate) {
    const birth = new Date(attrs.birthDate);
    if (!isNaN(birth.getTime())) {
      const now = new Date();
      const months = (now.getFullYear() - birth.getFullYear()) * 12
                   + (now.getMonth() - birth.getMonth());
      if (months >= 0 && months < 360) { // 0 to 30 years
        return { ageMonths: months, birthDateStr: attrs.birthDate };
      }
    }
  }

  // Strategy 2+3: Parse from ageString or description
  const ageMonths = parseAgeMonths(attrs.ageString, description);
  return { ageMonths, birthDateStr: attrs.birthDate || null };
}

export function mapRescueGroupsDog(
  externalId: string,
  attrs: RGAnimalAttributes,
  photoData?: { photos: string[]; primary: string | null; thumbnail: string | null }
): MappedDog {
  // Clean description: strip HTML entities
  let description = attrs.descriptionText || null;
  if (description) {
    description = description
      .replace(/&nbsp;/g, " ")
      .replace(/&rsquo;/g, "'")
      .replace(/&mdash;/g, "—")
      .replace(/&#39;/g, "'")
      .replace(/&amp;/g, "&")
      .trim();
  }

  const { ageMonths, birthDateStr } = computeAgeMonthsFromSources(attrs, description);
  const intakeDateResult = computeIntakeDate(attrs, description, ageMonths, birthDateStr);

  return {
    name: attrs.name || "Unknown",
    breed_primary: attrs.breedPrimary || "Mixed Breed",
    breed_secondary: attrs.breedSecondary || null,
    breed_mixed: attrs.isBreedMixed ?? false,
    size: SIZE_MAP[attrs.sizeGroup] || "medium",
    age_category: AGE_MAP[attrs.ageGroup] || "adult",
    age_months: ageMonths,
    gender: GENDER_MAP[attrs.sex] || "male",
    color_primary: null, // colors come from included data, not directly in attrs
    description,
    primary_photo_url: photoData?.primary || attrs.pictureThumbnailUrl || null,
    photo_urls: photoData?.photos || [],
    thumbnail_url: photoData?.thumbnail || attrs.pictureThumbnailUrl || null,
    status: attrs.isAdoptionPending ? "pending" : "available",
    is_spayed_neutered: attrs.isAltered ?? false,
    house_trained: attrs.isHousetrained ?? null,
    has_special_needs: attrs.isSpecialNeeds ?? false,
    good_with_kids: attrs.isKidsOk ?? null,
    good_with_dogs: attrs.isDogsOk ?? null,
    good_with_cats: attrs.isCatsOk ?? null,
    energy_level: (attrs.energyLevel && ENERGY_MAP[attrs.energyLevel]) || null,
    tags: attrs.qualities || [],
    ...intakeDateResult,
    birth_date: birthDateStr,
    is_birth_date_exact: attrs.isBirthDateExact ?? null,
    is_courtesy_listing: attrs.isCourtesyListing ?? false,
    external_id: externalId,
    external_source: "rescuegroups",
    external_url: attrs.url || null,
    last_synced_at: new Date().toISOString(),
  };
}

/**
 * Compute intake_date with confidence scoring + age sanity check.
 * Priority order:
 *   1. Date parsed from description text (e.g. "Posted 2/18/18") — highest confidence
 *   2. RescueGroups createdDate — most likely close to actual intake
 *   3. RescueGroups updatedDate — weaker signal (just means listing was edited)
 *   4. Current time — unknown confidence
 *
 * After computing, applies age sanity check:
 *   If wait time > dog's age, the intake_date is impossible and gets capped
 *   to the dog's estimated birth date.
 */
function computeIntakeDate(
  attrs: RGAnimalAttributes,
  description?: string | null,
  ageMonths?: number | null,
  birthDateStr?: string | null,
): {
  intake_date: string;
  date_confidence: DateConfidence;
  date_source: string;
} {
  const now = new Date();

  // Strategy 1: Parse a real date from the description text
  const descDate = parseDateFromDescription(description ?? null);
  if (descDate) {
    const result = {
      intake_date: descDate.date.toISOString(),
      date_confidence: (descDate.confidence === "high" ? "verified" : "high") as DateConfidence,
      date_source: `description_parsed: ${descDate.source}`,
    };
    return applyAgeSanityCheck(result, ageMonths, now, birthDateStr);
  }

  const createdDate = attrs.createdDate ? new Date(attrs.createdDate) : null;
  const updatedDate = attrs.updatedDate ? new Date(attrs.updatedDate) : null;

  // Courtesy listings: dates reflect the courtesy post, not shelter intake
  const isCourtesy = attrs.isCourtesyListing ?? false;

  // Strategy 2: createdDate — listing created when dog entered the system
  if (createdDate && !isNaN(createdDate.getTime())) {
    const daysSince = (now.getTime() - createdDate.getTime()) / (1000 * 60 * 60 * 24);

    if (daysSince < 0) {
      return applyAgeSanityCheck({
        intake_date: now.toISOString(),
        date_confidence: "low",
        date_source: "rescuegroups_created_date_future_corrected",
      }, ageMonths, now, birthDateStr);
    }

    if (isCourtesy) {
      // Courtesy listing dates are unreliable as intake proxies
      return applyAgeSanityCheck({
        intake_date: attrs.createdDate!,
        date_confidence: "low",
        date_source: "rescuegroups_courtesy_listing_created_date",
      }, ageMonths, now, birthDateStr);
    }

    // Check if the listing is actively maintained (updated recently)
    const isActive = updatedDate &&
      (now.getTime() - updatedDate.getTime()) / (1000 * 60 * 60 * 24) < 180;

    let confidence: DateConfidence;
    if (daysSince <= 90) confidence = "high";
    else if (daysSince <= 365) confidence = isActive ? "high" : "medium";
    else if (daysSince <= 730) confidence = "medium";
    else confidence = "low";

    return applyAgeSanityCheck({
      intake_date: attrs.createdDate!,
      date_confidence: confidence,
      date_source: `rescuegroups_created_date${isActive ? "_active" : ""}`,
    }, ageMonths, now, birthDateStr);
  }

  // Strategy 3: updatedDate only (no createdDate — shouldn't happen but handle gracefully)
  if (updatedDate && !isNaN(updatedDate.getTime())) {
    const daysSince = (now.getTime() - updatedDate.getTime()) / (1000 * 60 * 60 * 24);

    if (daysSince < 0) {
      return applyAgeSanityCheck({
        intake_date: now.toISOString(),
        date_confidence: "low",
        date_source: "rescuegroups_updated_date_future_corrected",
      }, ageMonths, now, birthDateStr);
    }

    return applyAgeSanityCheck({
      intake_date: attrs.updatedDate!,
      date_confidence: daysSince <= 180 ? "medium" : "low",
      date_source: daysSince <= 180 ? "rescuegroups_updated_date_only" : "rescuegroups_updated_date_stale",
    }, ageMonths, now, birthDateStr);
  }

  // No dates at all — use current time
  return applyAgeSanityCheck({
    intake_date: now.toISOString(),
    date_confidence: "unknown",
    date_source: "no_date_available",
  }, ageMonths, now, birthDateStr);
}

/**
 * Age sanity check: a dog cannot have been waiting longer than it's been alive.
 * Uses birthDate from RG API if available (gold standard), else estimates from ageMonths.
 * If intake_date is before birth, cap to birth date.
 */
function applyAgeSanityCheck(
  result: { intake_date: string; date_confidence: DateConfidence; date_source: string },
  ageMonths: number | null | undefined,
  now: Date,
  birthDateStr?: string | null,
): { intake_date: string; date_confidence: DateConfidence; date_source: string } {
  // Determine birth date: prefer API birthDate, fall back to ageMonths estimate
  let birthDate: Date | null = null;
  let birthSource = "estimated";

  if (birthDateStr) {
    const parsed = new Date(birthDateStr);
    if (!isNaN(parsed.getTime()) && parsed < now) {
      birthDate = parsed;
      birthSource = "api";
    }
  }

  if (!birthDate && ageMonths && ageMonths > 0) {
    birthDate = new Date(now);
    birthDate.setMonth(birthDate.getMonth() - ageMonths);
  }

  if (!birthDate) return result;

  const intakeDate = new Date(result.intake_date);
  if (isNaN(intakeDate.getTime())) return result;

  if (intakeDate < birthDate) {
    return {
      intake_date: birthDate.toISOString(),
      date_confidence: "low",
      date_source: `${result.date_source}|age_capped_${birthSource}`,
    };
  }

  return result;
}
