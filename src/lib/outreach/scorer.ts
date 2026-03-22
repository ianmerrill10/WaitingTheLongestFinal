/**
 * Priority scoring for shelter outreach.
 *
 * Higher score = higher priority for outreach.
 * Maximum possible score: ~130
 */

interface ShelterForScoring {
  shelter_type?: string;
  total_dogs?: number;
  website?: string;
  email?: string;
  phone?: string;
  state?: string;
  city?: string;
  has_euthanasia_dogs?: boolean;
}

// States with historically high euthanasia rates
const HIGH_EUTHANASIA_STATES = [
  "TX", "CA", "FL", "NC", "GA", "AL", "MS", "LA", "SC", "TN",
  "OK", "AR", "KY", "MO", "IN",
];

/**
 * Compute outreach priority score for a shelter.
 */
export function computePriorityScore(shelter: ShelterForScoring): {
  score: number;
  tier: "tier1" | "tier2" | "tier3" | "tier4";
  factors: string[];
} {
  let score = 0;
  const factors: string[] = [];

  // Kill shelter / government shelter (+50)
  const shelterType = (shelter.shelter_type || "").toLowerCase();
  if (
    shelterType.includes("kill") ||
    shelterType.includes("municipal") ||
    shelterType.includes("government") ||
    shelterType.includes("animal control")
  ) {
    score += 50;
    factors.push("kill_shelter:+50");
  }

  // Has euthanasia dogs (+30)
  if (shelter.has_euthanasia_dogs) {
    score += 30;
    factors.push("euthanasia_dogs:+30");
  }

  // High dog count (+20)
  const dogCount = shelter.total_dogs || 0;
  if (dogCount > 100) {
    score += 25;
    factors.push("dogs_100+:+25");
  } else if (dogCount > 50) {
    score += 20;
    factors.push("dogs_50+:+20");
  } else if (dogCount > 20) {
    score += 10;
    factors.push("dogs_20+:+10");
  }

  // Has website (+10)
  if (shelter.website) {
    score += 10;
    factors.push("has_website:+10");
  }

  // Has email (+10)
  if (shelter.email) {
    score += 10;
    factors.push("has_email:+10");
  }

  // High euthanasia state (+15)
  if (shelter.state && HIGH_EUTHANASIA_STATES.includes(shelter.state)) {
    score += 15;
    factors.push("high_euth_state:+15");
  }

  // Determine tier
  let tier: "tier1" | "tier2" | "tier3" | "tier4";
  if (score >= 70) {
    tier = "tier1";
  } else if (score >= 40) {
    tier = "tier2";
  } else if (score >= 20) {
    tier = "tier3";
  } else {
    tier = "tier4";
  }

  return { score, tier, factors };
}

/**
 * Determine outreach priority level from score.
 */
export function scoreToPriority(
  score: number
): "urgent" | "high" | "normal" | "low" {
  if (score >= 70) return "urgent";
  if (score >= 40) return "high";
  if (score >= 20) return "normal";
  return "low";
}

/**
 * Estimate shelter tier distribution.
 */
export const TIER_ESTIMATES = {
  tier1: { count: 1500, description: "Kill shelters — highest urgency" },
  tier2: { count: 3000, description: "High-volume shelters (50+ dogs)" },
  tier3: { count: 10000, description: "Active rescues — most likely to engage" },
  tier4: { count: 30000, description: "Smaller rescues and foster networks" },
};
