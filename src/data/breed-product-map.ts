/**
 * Breed-to-product recommendation mappings.
 *
 * Maps dog attributes (breed, size, age, conditions) to product categories
 * and specific product recommendations for affiliate monetization.
 */

export interface BreedRecommendation {
  categories: string[];
  keywords: string[];
  notes?: string;
}

/** Size-based product recommendations */
export const SIZE_RECOMMENDATIONS: Record<string, BreedRecommendation> = {
  small: {
    categories: ["harness", "carrier", "dental", "toys", "beds"],
    keywords: [
      "small dog harness",
      "pet carrier",
      "dental chews small breed",
      "small dog toys",
      "small dog bed",
    ],
    notes: "Focus on portability and dental health",
  },
  medium: {
    categories: ["crate", "bed", "leash", "toys", "food"],
    keywords: [
      "medium dog crate",
      "medium dog bed",
      "standard leash",
      "interactive dog toys",
      "medium breed dog food",
    ],
  },
  large: {
    categories: ["leash", "bed", "food_bowl", "toys", "supplements"],
    keywords: [
      "heavy-duty dog leash",
      "large orthopedic dog bed",
      "raised food bowls",
      "chew-resistant toys",
      "joint supplements large breed",
    ],
  },
  xlarge: {
    categories: ["crate", "bed", "leash", "food_bowl", "supplements"],
    keywords: [
      "extra-large dog crate",
      "XL dog bed",
      "gentle leader headcollar",
      "slow-feed dog bowl",
      "hip and joint supplements",
    ],
  },
};

/** Breed-specific product recommendations */
export const BREED_RECOMMENDATIONS: Record<string, BreedRecommendation> = {
  // Northern / Working breeds
  husky: {
    categories: ["grooming", "cooling", "harness", "food"],
    keywords: [
      "deshedding tool",
      "cooling mat",
      "running harness",
      "high-protein dog food",
      "undercoat rake",
    ],
  },
  malamute: {
    categories: ["grooming", "cooling", "harness", "food"],
    keywords: [
      "deshedding tool",
      "cooling mat",
      "sled harness",
      "high-protein dog food",
    ],
  },

  // Brachycephalic breeds
  bulldog: {
    categories: ["cooling", "health", "bed", "harness"],
    keywords: [
      "cooling vest",
      "wrinkle wipes",
      "orthopedic dog bed",
      "no-pull harness",
      "slow-feed bowl",
    ],
    notes: "Never recommend collars — always harnesses",
  },
  pug: {
    categories: ["cooling", "health", "harness", "grooming"],
    keywords: [
      "cooling vest",
      "wrinkle wipes",
      "pug harness",
      "eye wipes",
      "orthopedic bed",
    ],
  },
  "french bulldog": {
    categories: ["cooling", "health", "harness", "grooming"],
    keywords: [
      "cooling vest",
      "ear cleaner",
      "harness",
      "orthopedic bed",
      "wrinkle cream",
    ],
  },

  // Retrievers
  labrador: {
    categories: ["toys", "swimming", "supplements", "training"],
    keywords: [
      "fetch toys",
      "dog life jacket",
      "joint supplements",
      "training treats",
      "durable chew toys",
    ],
  },
  "golden retriever": {
    categories: ["grooming", "toys", "supplements", "swimming"],
    keywords: [
      "deshedding tool",
      "fetch toys",
      "dog life jacket",
      "joint supplements",
      "training treats",
    ],
  },

  // Toy breeds
  chihuahua: {
    categories: ["clothing", "carrier", "dental", "calming"],
    keywords: [
      "dog sweater small",
      "small dog carrier",
      "dental care kit",
      "calming treats",
      "small dog harness",
    ],
  },
  "yorkshire terrier": {
    categories: ["grooming", "carrier", "dental", "clothing"],
    keywords: [
      "dog grooming kit",
      "pet carrier",
      "dental chews small",
      "dog sweater",
    ],
  },

  // Bully breeds
  "pit bull": {
    categories: ["toys", "training", "harness", "bed"],
    keywords: [
      "heavy-duty dog toys",
      "training harness",
      "positive reinforcement tools",
      "durable dog bed",
      "chew-proof toys",
    ],
  },
  "american staffordshire terrier": {
    categories: ["toys", "training", "harness", "bed"],
    keywords: [
      "heavy-duty chew toys",
      "training harness",
      "large dog bed",
      "interactive toys",
    ],
  },

  // Long-body breeds
  dachshund: {
    categories: ["ramp", "harness", "supplements", "bed"],
    keywords: [
      "dog ramp for couch",
      "dachshund harness",
      "joint supplements",
      "orthopedic bed",
      "back support",
    ],
    notes: "Back health is critical — recommend ramps over stairs",
  },
  corgi: {
    categories: ["ramp", "harness", "supplements", "toys"],
    keywords: [
      "pet ramp",
      "corgi harness",
      "joint supplements",
      "herding ball",
      "puzzle toys",
    ],
  },

  // Guardian breeds
  "german shepherd": {
    categories: ["toys", "crate", "supplements", "training"],
    keywords: [
      "puzzle toys",
      "large training crate",
      "hip and joint supplements",
      "training treats",
      "dental chews",
    ],
  },
  doberman: {
    categories: ["toys", "training", "supplements", "bed"],
    keywords: [
      "puzzle toys",
      "training supplies",
      "joint supplements",
      "large dog bed",
    ],
  },
  rottweiler: {
    categories: ["toys", "training", "supplements", "leash"],
    keywords: [
      "heavy-duty toys",
      "training harness",
      "joint supplements",
      "strong leash",
    ],
  },

  // Herding breeds
  "border collie": {
    categories: ["toys", "agility", "food", "puzzle"],
    keywords: [
      "agility equipment",
      "puzzle feeders",
      "high-energy dog food",
      "herding ball",
      "interactive toys",
    ],
  },
  "australian cattle dog": {
    categories: ["toys", "agility", "food", "puzzle"],
    keywords: [
      "puzzle feeders",
      "herding ball",
      "high-energy food",
      "durable toys",
    ],
  },
  "australian shepherd": {
    categories: ["grooming", "agility", "toys", "food"],
    keywords: [
      "deshedding tool",
      "agility set",
      "frisbee",
      "high-energy dog food",
    ],
  },

  // Hounds
  beagle: {
    categories: ["toys", "harness", "training", "food"],
    keywords: [
      "scent toys",
      "no-pull harness",
      "training treats",
      "slow-feed bowl",
      "puzzle toys",
    ],
  },
  greyhound: {
    categories: ["bed", "clothing", "leash", "dental"],
    keywords: [
      "orthopedic dog bed",
      "dog coat winter",
      "martingale collar",
      "dental care",
      "elevated dog bed",
    ],
  },
};

/** Age-based product recommendations */
export const AGE_RECOMMENDATIONS: Record<string, BreedRecommendation> = {
  puppy: {
    categories: ["training", "teething", "food", "crate", "cleaning"],
    keywords: [
      "teething toys",
      "training pads",
      "puppy food",
      "crate training supplies",
      "enzyme cleaner",
      "puppy treats",
    ],
  },
  young: {
    categories: ["training", "toys", "food", "leash"],
    keywords: [
      "training treats",
      "durable toys",
      "young adult dog food",
      "training leash",
    ],
  },
  adult: {
    categories: ["food", "toys", "bed", "dental"],
    keywords: [
      "adult dog food",
      "interactive toys",
      "dog bed",
      "dental chews",
    ],
  },
  senior: {
    categories: ["bed", "supplements", "food", "ramp", "calming"],
    keywords: [
      "orthopedic dog bed",
      "joint supplements",
      "senior dog food",
      "pet ramp stairs",
      "calming supplements",
      "memory foam bed",
    ],
  },
};

/** Medical condition recommendations */
export const CONDITION_RECOMMENDATIONS: Record<string, BreedRecommendation> = {
  heartworm: {
    categories: ["health", "supplements"],
    keywords: [
      "heartworm prevention",
      "immune support supplements",
      "recovery food",
    ],
  },
  special_needs: {
    categories: ["mobility", "health", "adaptive"],
    keywords: [
      "dog wheelchair",
      "mobility harness",
      "orthopedic support",
      "adaptive equipment",
    ],
  },
  anxiety: {
    categories: ["calming", "bed", "supplements", "toys"],
    keywords: [
      "calming dog bed",
      "ThunderShirt",
      "calming supplements",
      "white noise machine",
      "snuffle mat",
    ],
  },
  high_energy: {
    categories: ["toys", "agility", "food", "puzzle"],
    keywords: [
      "puzzle toys",
      "agility equipment",
      "high-energy food",
      "fetch toys",
    ],
  },
};

/**
 * Get all matching recommendations for a dog.
 */
export function getRecommendationsForDog(dog: {
  breed?: string;
  size?: string;
  age_text?: string;
  description?: string;
}): {
  categories: string[];
  keywords: string[];
} {
  const categories = new Set<string>();
  const keywords = new Set<string>();

  // Size-based
  const sizeKey = normalizeSize(dog.size);
  if (sizeKey && SIZE_RECOMMENDATIONS[sizeKey]) {
    SIZE_RECOMMENDATIONS[sizeKey].categories.forEach((c) => categories.add(c));
    SIZE_RECOMMENDATIONS[sizeKey].keywords.forEach((k) => keywords.add(k));
  }

  // Breed-based
  const breedKey = normalizeBreed(dog.breed);
  if (breedKey && BREED_RECOMMENDATIONS[breedKey]) {
    BREED_RECOMMENDATIONS[breedKey].categories.forEach((c) =>
      categories.add(c)
    );
    BREED_RECOMMENDATIONS[breedKey].keywords.forEach((k) => keywords.add(k));
  }

  // Age-based
  const ageKey = normalizeAge(dog.age_text);
  if (ageKey && AGE_RECOMMENDATIONS[ageKey]) {
    AGE_RECOMMENDATIONS[ageKey].categories.forEach((c) => categories.add(c));
    AGE_RECOMMENDATIONS[ageKey].keywords.forEach((k) => keywords.add(k));
  }

  // Condition-based (from description)
  if (dog.description) {
    const desc = dog.description.toLowerCase();
    if (desc.includes("heartworm")) {
      CONDITION_RECOMMENDATIONS.heartworm.categories.forEach((c) =>
        categories.add(c)
      );
      CONDITION_RECOMMENDATIONS.heartworm.keywords.forEach((k) =>
        keywords.add(k)
      );
    }
    if (
      desc.includes("special needs") ||
      desc.includes("wheelchair") ||
      desc.includes("blind") ||
      desc.includes("deaf")
    ) {
      CONDITION_RECOMMENDATIONS.special_needs.categories.forEach((c) =>
        categories.add(c)
      );
      CONDITION_RECOMMENDATIONS.special_needs.keywords.forEach((k) =>
        keywords.add(k)
      );
    }
    if (desc.includes("anxiety") || desc.includes("fearful") || desc.includes("nervous")) {
      CONDITION_RECOMMENDATIONS.anxiety.categories.forEach((c) =>
        categories.add(c)
      );
      CONDITION_RECOMMENDATIONS.anxiety.keywords.forEach((k) =>
        keywords.add(k)
      );
    }
  }

  return {
    categories: Array.from(categories),
    keywords: Array.from(keywords),
  };
}

function normalizeSize(size?: string): string | null {
  if (!size) return null;
  const s = size.toLowerCase();
  if (s.includes("small") || s.includes("toy") || s.includes("mini"))
    return "small";
  if (s.includes("medium") || s.includes("mid")) return "medium";
  if (s.includes("xlarge") || s.includes("x-large") || s.includes("giant"))
    return "xlarge";
  if (s.includes("large")) return "large";
  return null;
}

function normalizeBreed(breed?: string): string | null {
  if (!breed) return null;
  const b = breed.toLowerCase().trim();

  // Direct matches
  if (BREED_RECOMMENDATIONS[b]) return b;

  // Partial matches
  for (const key of Object.keys(BREED_RECOMMENDATIONS)) {
    if (b.includes(key) || key.includes(b)) return key;
  }

  return null;
}

function normalizeAge(ageText?: string): string | null {
  if (!ageText) return null;
  const a = ageText.toLowerCase();
  if (a.includes("puppy") || a.includes("baby")) return "puppy";
  if (a.includes("young")) return "young";
  if (a.includes("senior") || a.includes("old")) return "senior";
  return "adult";
}
