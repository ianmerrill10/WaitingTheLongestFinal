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
  euthanasia_date: string | null;
  is_on_euthanasia_list: boolean;
  external_id: string;
  external_source: string;
  external_url: string | null;
  last_synced_at: string;
  source_extraction_method: string;
  is_foster: boolean;
  intake_type: string | null;
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

  // Foster detection from description, tags, or status text
  const isFoster = /\b(foster home|in foster|currently fostered|living in foster)\b/i.test(description || "")
    || (attrs.qualities || []).some((q: string) => /foster/i.test(q))
    || (attrs.isAdoptionPending === false && /foster/i.test(attrs.name || ""));

  // Intake type detection from RG attributes
  let intakeType: string | null = null;
  if (attrs.foundDate) intakeType = "stray";
  else if (/\b(surrender|owner.?surrender|relinquish)\b/i.test(description || "")) intakeType = "owner_surrender";
  else if (/\b(transfer|transferred from)\b/i.test(description || "")) intakeType = "transfer";

  // NOTE: RescueGroups killDate is NOT used for euthanasia countdowns.
  // The field is unverifiable — it could mean a review date, a stale default,
  // or something other than actual euthanasia. We only show euthanasia
  // countdowns from verified sources (scraped at-risk pages, direct shelter feeds).
  const euthanasia_date: string | null = null;
  const is_on_euthanasia_list = false;

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
    euthanasia_date,
    is_on_euthanasia_list,
    external_id: externalId,
    external_source: "rescuegroups",
    external_url: attrs.url || null,
    last_synced_at: new Date().toISOString(),
    source_extraction_method: "rescuegroups_api_v5",
    is_foster: isFoster,
    intake_type: intakeType,
  };
}

/**
 * Compute intake_date with confidence scoring + age sanity check.
 *
 * PRIORITY ORDER (verified > API-provided > inferred):
 *   1. RescueGroups availableDate — shelter-set "available for adoption" date (GOLD STANDARD)
 *   2. RescueGroups foundDate — when stray was found (= intake for strays)
 *   3. Date parsed from description text (e.g. "Posted 2/18/18")
 *   4. RescueGroups createdDate — when listing was created on RG platform
 *   5. RescueGroups updatedDate — fallback (listing last edited)
 *   6. Current time — unknown confidence
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

  // Helper: validate a date string is real and not in the future
  function validDate(dateStr?: string | null): Date | null {
    if (!dateStr) return null;
    const d = new Date(dateStr);
    if (isNaN(d.getTime())) return null;
    if (d > now) return null; // future dates invalid
    return d;
  }

  // Courtesy listings: dates reflect the courtesy post, not shelter intake
  const isCourtesy = attrs.isCourtesyListing ?? false;

  // ─── RETURNED DOG CHECK ───
  // If adoptedDate exists AND is before updatedDate, this dog was adopted and returned.
  // The real "waiting since" is when they came back, not when they were first listed.
  const adoptedDate = validDate(attrs.adoptedDate);
  const returnedCheckDate = validDate(attrs.updatedDate);
  if (adoptedDate && returnedCheckDate && returnedCheckDate > adoptedDate && !isCourtesy) {
    // Dog was adopted then returned — wait time starts from the update after adoption
    return applyAgeSanityCheck({
      intake_date: attrs.updatedDate!,
      date_confidence: "high",
      date_source: "rescuegroups_returned_after_adoption",
    }, ageMonths, now, birthDateStr);
  }

  // ─── STRATEGY 1: availableDate (shelter-provided "available for adoption" date) ───
  const availDate = validDate(attrs.availableDate);
  if (availDate && !isCourtesy) {
    return applyAgeSanityCheck({
      intake_date: attrs.availableDate!,
      date_confidence: "verified",
      date_source: "rescuegroups_available_date",
    }, ageMonths, now, birthDateStr);
  }

  // ─── STRATEGY 2: foundDate (when stray was found = intake date for strays) ───
  const foundDate = validDate(attrs.foundDate);
  if (foundDate && !isCourtesy) {
    return applyAgeSanityCheck({
      intake_date: attrs.foundDate!,
      date_confidence: "verified",
      date_source: "rescuegroups_found_date",
    }, ageMonths, now, birthDateStr);
  }

  // ─── STRATEGY 3: Parse a real date from the description text ───
  const descDate = parseDateFromDescription(description ?? null);
  if (descDate) {
    return applyAgeSanityCheck({
      intake_date: descDate.date.toISOString(),
      date_confidence: (descDate.confidence === "high" ? "verified" : "high") as DateConfidence,
      date_source: `description_parsed: ${descDate.source}`,
    }, ageMonths, now, birthDateStr);
  }

  // ─── STRATEGY 4: createdDate (listing creation on RescueGroups) ───
  const createdDate = validDate(attrs.createdDate);
  if (createdDate) {
    const daysSince = (now.getTime() - createdDate.getTime()) / (1000 * 60 * 60 * 24);

    if (isCourtesy) {
      return applyAgeSanityCheck({
        intake_date: attrs.createdDate!,
        date_confidence: "low",
        date_source: "rescuegroups_courtesy_listing_created_date",
      }, ageMonths, now, birthDateStr);
    }

    // Check if the listing is actively maintained (updated recently)
    const updatedDate = validDate(attrs.updatedDate);
    const isActive = updatedDate &&
      (now.getTime() - updatedDate.getTime()) / (1000 * 60 * 60 * 24) < 180;

    let confidence: DateConfidence;
    if (daysSince <= 90) confidence = "high";
    else if (daysSince <= 365) confidence = isActive ? "high" : "medium";
    else if (daysSince <= 730) confidence = isActive ? "medium" : "low";
    else confidence = "low"; // >2 years: listing creation date is NOT a reliable intake date

    return applyAgeSanityCheck({
      intake_date: attrs.createdDate!,
      date_confidence: confidence,
      date_source: `rescuegroups_created_date${isActive ? "_active" : ""}`,
    }, ageMonths, now, birthDateStr);
  }

  // ─── STRATEGY 5: updatedDate only ───
  const updatedDate = validDate(attrs.updatedDate);
  if (updatedDate) {
    const daysSince = (now.getTime() - updatedDate.getTime()) / (1000 * 60 * 60 * 24);
    return applyAgeSanityCheck({
      intake_date: attrs.updatedDate!,
      date_confidence: daysSince <= 90 ? "medium" : "low",
      date_source: daysSince <= 90 ? "rescuegroups_updated_date_only" : "rescuegroups_updated_date_stale",
    }, ageMonths, now, birthDateStr);
  }

  // ─── STRATEGY 6: No dates at all ───
  return applyAgeSanityCheck({
    intake_date: now.toISOString(),
    date_confidence: "unknown",
    date_source: "no_date_available",
  }, ageMonths, now, birthDateStr);
}

// ─── Maximum reasonable wait time caps (in days) by confidence level ───
// Aggressive caps: the longest verified shelter waits on record are ~5 years.
// Listing creation dates older than 1-2 years are almost never real intake dates.
// Better to show "Estimated" with a shorter time than a wildly wrong 5-year claim.
export const MAX_WAIT_DAYS: Record<DateConfidence, number> = {
  verified: 2555, // 7 years — only verified shelter-provided dates get this long
  high: 1825,     // 5 years — strong evidence but could be stale listing
  medium: 1095,   // 3 years — moderate evidence, cap conservatively
  low: 365,       // 1 year — weak evidence, cap aggressively
  unknown: 180,   // 6 months — no evidence, assume recent
};

/**
 * Age sanity check + max wait time cap.
 *
 * Two checks:
 * 1. A dog cannot have been waiting longer than it's been alive.
 * 2. No wait time can exceed the max threshold for its confidence level.
 *    Even "verified" dates get capped — a listing active since 2009 is
 *    almost certainly a stale/orphaned listing, not a real 16-year wait.
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

  const intakeDate = new Date(result.intake_date);
  if (isNaN(intakeDate.getTime())) return result;

  // Check 1: Can't wait longer than you've been alive
  if (birthDate && intakeDate < birthDate) {
    result = {
      intake_date: birthDate.toISOString(),
      date_confidence: "low",
      date_source: `${result.date_source}|age_capped_${birthSource}`,
    };
  }

  // Check 2: Max wait time cap by confidence level
  const waitDays = (now.getTime() - new Date(result.intake_date).getTime()) / (1000 * 60 * 60 * 24);
  const maxDays = MAX_WAIT_DAYS[result.date_confidence] || MAX_WAIT_DAYS.unknown;

  if (waitDays > maxDays) {
    const cappedDate = new Date(now);
    cappedDate.setDate(cappedDate.getDate() - maxDays);
    return {
      intake_date: cappedDate.toISOString(),
      date_confidence: result.date_confidence === "verified" ? "medium" : "low",
      date_source: `${result.date_source}|wait_capped_${maxDays}d`,
    };
  }

  return result;
}
