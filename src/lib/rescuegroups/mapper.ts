// Maps RescueGroups v5 API data to our dog schema
// Based on real API response structure

import type { RGAnimalAttributes, RGIncludedItem } from "./client";

export type DateConfidence = "verified" | "high" | "medium" | "low" | "unknown";

export interface MappedDog {
  name: string;
  breed_primary: string;
  breed_secondary: string | null;
  breed_mixed: boolean;
  size: "small" | "medium" | "large" | "xlarge";
  age_category: "puppy" | "young" | "adult" | "senior";
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

  return {
    name: attrs.name || "Unknown",
    breed_primary: attrs.breedPrimary || "Mixed Breed",
    breed_secondary: attrs.breedSecondary || null,
    breed_mixed: attrs.isBreedMixed ?? false,
    size: SIZE_MAP[attrs.sizeGroup] || "medium",
    age_category: AGE_MAP[attrs.ageGroup] || "adult",
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
    ...computeIntakeDate(attrs),
    external_id: externalId,
    external_source: "rescuegroups",
    external_url: attrs.url || null,
    last_synced_at: new Date().toISOString(),
  };
}

/**
 * Compute intake_date with confidence scoring.
 * RescueGroups provides createdDate (listing creation) and updatedDate (last edit).
 * Neither is the actual shelter intake date — we score confidence accordingly.
 */
function computeIntakeDate(attrs: RGAnimalAttributes): {
  intake_date: string;
  date_confidence: DateConfidence;
  date_source: string;
} {
  const now = new Date();
  const updatedDate = attrs.updatedDate ? new Date(attrs.updatedDate) : null;
  const createdDate = attrs.createdDate ? new Date(attrs.createdDate) : null;

  // Prefer updatedDate — it's when the shelter last touched the record
  if (updatedDate && !isNaN(updatedDate.getTime())) {
    const daysSince = (now.getTime() - updatedDate.getTime()) / (1000 * 60 * 60 * 24);

    // Future date — clearly wrong
    if (daysSince < 0) {
      return {
        intake_date: now.toISOString(),
        date_confidence: "low",
        date_source: "rescuegroups_updated_date_future_corrected",
      };
    }

    // Within 6 months — high confidence
    if (daysSince <= 180) {
      return {
        intake_date: attrs.updatedDate!,
        date_confidence: "high",
        date_source: "rescuegroups_updated_date",
      };
    }

    // 6 months to 2 years — medium
    if (daysSince <= 730) {
      return {
        intake_date: attrs.updatedDate!,
        date_confidence: "medium",
        date_source: "rescuegroups_updated_date",
      };
    }

    // Over 2 years — low confidence, use it but flag it
    return {
      intake_date: attrs.updatedDate!,
      date_confidence: "low",
      date_source: "rescuegroups_updated_date_stale",
    };
  }

  // Fallback to createdDate — even less reliable
  if (createdDate && !isNaN(createdDate.getTime())) {
    const daysSince = (now.getTime() - createdDate.getTime()) / (1000 * 60 * 60 * 24);

    if (daysSince < 0) {
      return {
        intake_date: now.toISOString(),
        date_confidence: "low",
        date_source: "rescuegroups_created_date_future_corrected",
      };
    }

    // createdDate within 6 months — medium (not high, since it's listing creation not intake)
    if (daysSince <= 180) {
      return {
        intake_date: attrs.createdDate!,
        date_confidence: "medium",
        date_source: "rescuegroups_created_date",
      };
    }

    // Over 6 months — low confidence
    return {
      intake_date: attrs.createdDate!,
      date_confidence: "low",
      date_source: "rescuegroups_created_date_stale",
    };
  }

  // No dates at all — use current time
  return {
    intake_date: now.toISOString(),
    date_confidence: "unknown",
    date_source: "no_date_available",
  };
}
