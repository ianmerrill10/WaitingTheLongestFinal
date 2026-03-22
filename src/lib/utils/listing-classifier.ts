/**
 * Listing Classifier — precision-first dog listing validation
 *
 * Rejects breeders, pet stores, and for-profit sellers.
 * Validates data quality. Normalizes inconsistent fields.
 * Prioritizes precision over recall: better to miss a dog than include a bad one.
 */

// ─── Source classification ───

export type SourceClass = "rescue" | "shelter" | "unknown" | "breeder";

export interface ClassificationResult {
  classification: SourceClass;
  confidence: number; // 0.0 - 1.0
  reason: string;
  shouldReject: boolean;
}

export interface ListingValidation {
  isValid: boolean;
  classification: ClassificationResult;
  issues: string[];
  normalizedData: NormalizedFields | null;
}

export interface NormalizedFields {
  name: string;
  breed: string;
  size: "small" | "medium" | "large" | "xlarge" | "unknown";
  age_years: number | null;
  age_category: "puppy" | "young" | "adult" | "senior" | "unknown";
  sex: "male" | "female" | "unknown";
  is_adoptable: boolean;
}

// ─── Breeder detection patterns ───

const BREEDER_SIGNALS = [
  // Commercial sale language
  { pattern: /\bfor sale\b/i, weight: 0.8, reason: "for-sale language" },
  { pattern: /\bprice\s*[:$]\s*\d/i, weight: 0.7, reason: "price listing" },
  { pattern: /\b(?:buy|purchase|order)\s+(?:your|a|this)\s+(?:puppy|pup|dog)\b/i, weight: 0.9, reason: "purchase language" },
  { pattern: /\bpuppies?\s+(?:available|for sale|ready)\s+now\b/i, weight: 0.8, reason: "puppies available now" },
  { pattern: /\b(?:champion|show quality|breeding quality)\b/i, weight: 0.6, reason: "breeding quality language" },

  // AKC/registration marketing
  { pattern: /\bAKC\s+registered\b/i, weight: 0.7, reason: "AKC registration marketing" },
  { pattern: /\bAKC\s+papers?\b/i, weight: 0.7, reason: "AKC papers marketing" },
  { pattern: /\b(?:full|limited)\s+registration\b/i, weight: 0.6, reason: "registration type marketing" },
  { pattern: /\bpedigree\s+papers?\b/i, weight: 0.6, reason: "pedigree papers" },

  // Multiple litters / commercial breeding
  { pattern: /\b(?:litter|litters)\s+(?:available|ready|born|due)\b/i, weight: 0.9, reason: "litter availability" },
  { pattern: /\bnew\s+litter\b/i, weight: 0.8, reason: "new litter announcement" },
  { pattern: /\b(?:stud|breeding)\s+(?:service|program|fee)\b/i, weight: 0.9, reason: "breeding service" },
  { pattern: /\bdam\s+and\s+sire\b/i, weight: 0.7, reason: "dam and sire reference" },
  { pattern: /\b(?:F1|F1B|F2|multi-gen)\s*(?:doodle|goldendoodle|labradoodle|bernedoodle|cavapoo|cockapoo)/i, weight: 0.8, reason: "designer breed generation marketing" },

  // Pet store / commercial seller
  { pattern: /\bpet\s+store\b/i, weight: 0.9, reason: "pet store" },
  { pattern: /\bfinancing\s+available\b/i, weight: 0.8, reason: "financing offered" },
  { pattern: /\bpayment\s+plan\b/i, weight: 0.7, reason: "payment plan offered" },
  { pattern: /\bshipping\s+(?:available|nationwide|worldwide)\b/i, weight: 0.8, reason: "shipping offered" },
  { pattern: /\b(?:deposit|reservation)\s+(?:fee|required|accepted)\b/i, weight: 0.5, reason: "deposit/reservation" },

  // High prices (adoption fees are typically <$500)
  { pattern: /\$\s*(?:1[5-9]\d\d|[2-9]\d{3}|\d{5,})\b/, weight: 0.7, reason: "price over $1500" },
  { pattern: /\b(?:1[5-9]\d\d|[2-9]\d{3})\s*(?:dollars?|usd)\b/i, weight: 0.7, reason: "high price mentioned" },

  // Guarantee language (breeders offer health guarantees)
  { pattern: /\b(?:health|genetic)\s+guarantee\b/i, weight: 0.4, reason: "health guarantee" },
  { pattern: /\byear\s+(?:health\s+)?guarantee\b/i, weight: 0.5, reason: "multi-year guarantee" },
];

// ─── Rescue/shelter signals ───

const RESCUE_SIGNALS = [
  { pattern: /\b(?:adopt|adoption)\b/i, weight: 0.6 },
  { pattern: /\b(?:rescue|rescued|foster|fostered)\b/i, weight: 0.7 },
  { pattern: /\b(?:shelter|humane society|spca|animal control)\b/i, weight: 0.8 },
  { pattern: /\b(?:nonprofit|non-profit|501\s*\(?\s*c\s*\)?\s*3)\b/i, weight: 0.9 },
  { pattern: /\badoption\s+fee\b/i, weight: 0.7 },
  { pattern: /\b(?:spayed?|neutered?|altered|microchipped|vaccinated|up\s+to\s+date)\b/i, weight: 0.4 },
  { pattern: /\b(?:surrender|intake|stray|found|owner release)\b/i, weight: 0.6 },
  { pattern: /\bforever\s+home\b/i, weight: 0.5 },
  { pattern: /\bapplication\s+(?:required|process)\b/i, weight: 0.6 },
  { pattern: /\bhome\s+(?:check|visit)\b/i, weight: 0.5 },
];

// ─── Rejection patterns (instant reject, no score needed) ───

const INSTANT_REJECT_PATTERNS = [
  /\bbreeders?\s+(?:with|of|since|for)\b/i,
  /\bwe\s+(?:breed|raise)\b/i,
  /\bour\s+(?:puppies|breeding\s+program)\b/i,
  /\bpuppies?\s+(?:\$|starting\s+at)\s*\d/i,
  /\b(?:rehoming|re-homing)\s+fee\s*[:$]?\s*(?:\$\s*)?\d{4,}/i, // "rehoming fee" over $1000
];

// ─── Invalid / spam name patterns ───

const INVALID_NAME_PATTERNS = [
  /^(?:test|asdf|xxx|sample|placeholder|foo|bar|temp)\s*\d*$/i,
  /^\d+$/, // Pure number names
  /^.{0,1}$/, // Single character or empty
  /^(?:no\s+name|unnamed|none|n\/a|tbd|unknown dog|dog)$/i,
];

// ─── Size normalization ───

function normalizeSize(
  sizeText: string | null | undefined,
  weightLbs: number | null | undefined,
): "small" | "medium" | "large" | "xlarge" | "unknown" {
  // Weight-based (most accurate)
  if (weightLbs != null) {
    if (weightLbs < 25) return "small";
    if (weightLbs < 50) return "medium";
    if (weightLbs < 90) return "large";
    return "xlarge";
  }

  // Text-based
  if (!sizeText) return "unknown";
  const lower = sizeText.toLowerCase().trim();

  if (lower.includes("small") || lower.includes("tiny") || lower.includes("mini") || lower.includes("toy")) return "small";
  if (lower.includes("x-large") || lower.includes("xlarge") || lower.includes("extra large") || lower.includes("giant")) return "xlarge";
  if (lower.includes("large")) return "large";
  if (lower.includes("medium") || lower.includes("mid")) return "medium";

  return "unknown";
}

// ─── Age normalization ───

function normalizeAge(
  ageText: string | null | undefined,
  ageMonths: number | null | undefined,
): { years: number | null; category: "puppy" | "young" | "adult" | "senior" | "unknown" } {
  let months: number | null = ageMonths ?? null;

  if (months == null && ageText) {
    const yearMatch = ageText.match(/(\d+)\s*(?:year|yr)/i);
    const monthMatch = ageText.match(/(\d+)\s*(?:month|mo)/i);
    const weekMatch = ageText.match(/(\d+)\s*(?:week|wk)/i);

    let total = 0;
    let found = false;

    if (yearMatch) { total += parseInt(yearMatch[1]) * 12; found = true; }
    if (monthMatch) { total += parseInt(monthMatch[1]); found = true; }
    if (weekMatch) { total += Math.round(parseInt(weekMatch[1]) / 4.33); found = true; }

    if (found && total > 0) months = total;
  }

  const years = months != null ? Math.round(months / 12 * 10) / 10 : null;

  let category: "puppy" | "young" | "adult" | "senior" | "unknown" = "unknown";
  if (months != null) {
    if (months < 12) category = "puppy";
    else if (months < 36) category = "young";
    else if (months < 84) category = "adult";
    else category = "senior";
  }

  return { years, category };
}

// ─── Sex normalization ───

function normalizeSex(sex: string | null | undefined): "male" | "female" | "unknown" {
  if (!sex) return "unknown";
  const lower = sex.toLowerCase().trim();
  if (lower === "male" || lower === "m" || lower === "boy") return "male";
  if (lower === "female" || lower === "f" || lower === "girl") return "female";
  return "unknown";
}

// ─── Name cleaning ───

function cleanName(name: string | null | undefined): string {
  if (!name) return "Unknown";

  let cleaned = name
    .replace(/<[^>]+>/g, "") // Strip HTML
    .replace(/&\w+;/g, " ") // Strip HTML entities
    .replace(/\s+/g, " ") // Collapse whitespace
    .trim();

  // Title case if all-caps
  if (cleaned === cleaned.toUpperCase() && cleaned.length > 2) {
    cleaned = cleaned.replace(/\b\w+/g, w =>
      w.charAt(0).toUpperCase() + w.slice(1).toLowerCase()
    );
  }

  return cleaned || "Unknown";
}

// ─── Main classification function ───

export function classifySource(
  description: string | null | undefined,
  orgName: string | null | undefined,
  orgType: string | null | undefined,
): ClassificationResult {
  const text = [description, orgName].filter(Boolean).join(" ");

  if (!text || text.length < 5) {
    return { classification: "unknown", confidence: 0.3, reason: "insufficient text", shouldReject: false };
  }

  // Instant reject check
  for (const pattern of INSTANT_REJECT_PATTERNS) {
    if (pattern.test(text)) {
      return {
        classification: "breeder",
        confidence: 0.95,
        reason: `Instant reject: ${pattern.source.slice(0, 50)}`,
        shouldReject: true,
      };
    }
  }

  // Score breeder signals
  let breederScore = 0;
  let breederReasons: string[] = [];
  for (const { pattern, weight, reason } of BREEDER_SIGNALS) {
    if (pattern.test(text)) {
      breederScore += weight;
      breederReasons.push(reason);
    }
  }

  // Score rescue signals
  let rescueScore = 0;
  for (const { pattern, weight } of RESCUE_SIGNALS) {
    if (pattern.test(text)) {
      rescueScore += weight;
    }
  }

  // Org type boost
  if (orgType) {
    const lower = orgType.toLowerCase();
    if (lower.includes("breeder")) {
      breederScore += 2.0;
      breederReasons.push("org type is breeder");
    }
    if (lower === "municipal" || lower.includes("shelter") || lower.includes("rescue") ||
        lower.includes("humane") || lower.includes("spca")) {
      rescueScore += 1.0;
    }
  }

  // Decision logic
  if (breederScore >= 1.5) {
    return {
      classification: "breeder",
      confidence: Math.min(0.95, 0.5 + breederScore * 0.15),
      reason: breederReasons.join(", "),
      shouldReject: true,
    };
  }

  // Breeder score > rescue score and meaningful breeder signal
  if (breederScore > rescueScore && breederScore >= 0.7) {
    return {
      classification: "breeder",
      confidence: Math.min(0.85, 0.4 + breederScore * 0.1),
      reason: breederReasons.join(", "),
      shouldReject: true,
    };
  }

  if (rescueScore >= 1.0) {
    const classification = rescueScore >= 1.5 ? "shelter" : "rescue";
    return {
      classification,
      confidence: Math.min(0.95, 0.5 + rescueScore * 0.1),
      reason: "rescue/shelter signals detected",
      shouldReject: false,
    };
  }

  return {
    classification: "unknown",
    confidence: 0.4,
    reason: "no strong signals",
    shouldReject: false,
  };
}

// ─── Full listing validation ───

export function validateListing(options: {
  name?: string | null;
  breed?: string | null;
  description?: string | null;
  size?: string | null;
  weightLbs?: number | null;
  ageText?: string | null;
  ageMonths?: number | null;
  sex?: string | null;
  orgName?: string | null;
  orgType?: string | null;
  adoptionFee?: number | null;
  status?: string | null;
}): ListingValidation {
  const issues: string[] = [];

  // 1. Classify source
  const classification = classifySource(options.description, options.orgName, options.orgType);

  if (classification.shouldReject) {
    return {
      isValid: false,
      classification,
      issues: [`REJECTED: ${classification.reason}`],
      normalizedData: null,
    };
  }

  // 2. Validate required fields
  const name = cleanName(options.name);

  // Check for invalid names
  for (const pattern of INVALID_NAME_PATTERNS) {
    if (pattern.test(name)) {
      issues.push(`Invalid name: "${name}"`);
      break;
    }
  }

  // Breed validation
  const breed = options.breed?.trim() || "Mixed Breed";
  if (!breed || breed.length < 2) {
    issues.push("Missing breed");
  }

  // 3. Normalize fields
  const size = normalizeSize(options.size, options.weightLbs);
  const { years: ageYears, category: ageCategory } = normalizeAge(options.ageText, options.ageMonths);
  const sex = normalizeSex(options.sex);

  // 4. Data quality checks

  // Adoption fee sanity: legitimate adoption fees are typically $0-$800
  if (options.adoptionFee != null && options.adoptionFee > 1000) {
    issues.push(`Suspiciously high adoption fee: $${options.adoptionFee}`);
    // If fee is very high, flag as possible breeder
    if (options.adoptionFee > 2000) {
      return {
        isValid: false,
        classification: {
          classification: "breeder",
          confidence: 0.7,
          reason: `Adoption fee $${options.adoptionFee} exceeds rescue threshold`,
          shouldReject: true,
        },
        issues: [`REJECTED: adoption fee $${options.adoptionFee} too high for rescue`],
        normalizedData: null,
      };
    }
  }

  // Age sanity: dogs over 25 years are almost certainly data errors
  if (ageYears != null && ageYears > 25) {
    issues.push(`Impossible age: ${ageYears} years`);
  }

  // Status check
  const isAdoptable = !options.status ||
    ["available", "adoptable", "active"].includes(options.status.toLowerCase());

  // 5. Determine validity
  const criticalIssues = issues.filter(i =>
    i.startsWith("REJECTED") || i.includes("Impossible age")
  );
  const isValid = criticalIssues.length === 0;

  return {
    isValid,
    classification,
    issues,
    normalizedData: {
      name,
      breed,
      size,
      age_years: ageYears,
      age_category: ageCategory,
      sex,
      is_adoptable: isAdoptable,
    },
  };
}

// ─── Batch classifier for descriptions already in DB ───

export function classifyDescription(description: string | null): ClassificationResult {
  return classifySource(description, null, null);
}
