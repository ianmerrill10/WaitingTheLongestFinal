// Maps RescueGroups API data to our dog schema
// Field mappings from WaitingTheLongestRedo/config/aggregators.json

interface RescueGroupsAnimalAttributes {
  name: string;
  breedPrimary: string;
  breedSecondary: string | null;
  isMixedBreed: boolean;
  ageGroup: string;
  sex: string;
  sizeGroup: string;
  colorDetails: string | null;
  descriptionText: string | null;
  pictureThumbnailUrl: string | null;
  adoptionStatus: string;
  isAltered: boolean;
  isHousetrained: boolean;
  isSpecialNeeds: boolean;
  isKidsOk: boolean | null;
  isDogsOk: boolean | null;
  isCatsOk: boolean | null;
  createdDate: string;
  url: string | null;
}

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
  status: string;
  is_spayed_neutered: boolean;
  house_trained: boolean | null;
  has_special_needs: boolean;
  good_with_kids: boolean | null;
  good_with_dogs: boolean | null;
  good_with_cats: boolean | null;
  intake_date: string;
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

export function mapRescueGroupsDog(
  externalId: string,
  attrs: RescueGroupsAnimalAttributes
): MappedDog {
  return {
    name: attrs.name || "Unknown",
    breed_primary: attrs.breedPrimary || "Mixed Breed",
    breed_secondary: attrs.breedSecondary || null,
    breed_mixed: attrs.isMixedBreed ?? false,
    size: SIZE_MAP[attrs.sizeGroup] || "medium",
    age_category: AGE_MAP[attrs.ageGroup] || "adult",
    gender: GENDER_MAP[attrs.sex] || "male",
    color_primary: attrs.colorDetails || null,
    description: attrs.descriptionText || null,
    primary_photo_url: attrs.pictureThumbnailUrl || null,
    status: attrs.adoptionStatus === "Available" ? "available" : "pending",
    is_spayed_neutered: attrs.isAltered ?? false,
    house_trained: attrs.isHousetrained ?? null,
    has_special_needs: attrs.isSpecialNeeds ?? false,
    good_with_kids: attrs.isKidsOk ?? null,
    good_with_dogs: attrs.isDogsOk ?? null,
    good_with_cats: attrs.isCatsOk ?? null,
    intake_date: attrs.createdDate || new Date().toISOString(),
    external_id: externalId,
    external_source: "rescuegroups",
    external_url: attrs.url || null,
    last_synced_at: new Date().toISOString(),
  };
}
