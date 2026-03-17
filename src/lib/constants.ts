// Urgency thresholds in hours
export const URGENCY_THRESHOLDS = {
  CRITICAL: 24, // < 24 hours
  HIGH: 72, // < 72 hours (3 days)
  MEDIUM: 168, // < 168 hours (7 days)
} as const;

export type UrgencyLevel = "critical" | "high" | "medium" | "normal";

// LED counter labels
export const WAIT_TIME_LABELS = ["YR", "MO", "DY", "HR", "MN", "SC"] as const;
export const COUNTDOWN_LABELS = ["DY", "HR", "MN", "SC"] as const;

// 7-segment display patterns: [a, b, c, d, e, f, g]
export const SEGMENT_PATTERNS: Record<string, number[]> = {
  "0": [1, 1, 1, 1, 1, 1, 0],
  "1": [0, 1, 1, 0, 0, 0, 0],
  "2": [1, 1, 0, 1, 1, 0, 1],
  "3": [1, 1, 1, 1, 0, 0, 1],
  "4": [0, 1, 1, 0, 0, 1, 1],
  "5": [1, 0, 1, 1, 0, 1, 1],
  "6": [1, 0, 1, 1, 1, 1, 1],
  "7": [1, 1, 1, 0, 0, 0, 0],
  "8": [1, 1, 1, 1, 1, 1, 1],
  "9": [1, 1, 1, 1, 0, 1, 1],
  "-": [0, 0, 0, 0, 0, 0, 1],
};

// Dog size categories
export const DOG_SIZES = ["small", "medium", "large", "xlarge"] as const;
export const DOG_SIZE_LABELS: Record<string, string> = {
  small: "Small (under 25 lbs)",
  medium: "Medium (25-50 lbs)",
  large: "Large (50-90 lbs)",
  xlarge: "Extra Large (90+ lbs)",
};

// Dog age categories
export const DOG_AGES = ["puppy", "young", "adult", "senior"] as const;
export const DOG_AGE_LABELS: Record<string, string> = {
  puppy: "Puppy (under 1 year)",
  young: "Young (1-3 years)",
  adult: "Adult (3-7 years)",
  senior: "Senior (7+ years)",
};

// Pagination
export const DOGS_PER_PAGE = 24;

// RescueGroups API
export const RESCUEGROUPS_API_BASE = "https://api.rescuegroups.org/v5";
export const RESCUEGROUPS_RATE_LIMIT = {
  requestsPerSecond: 2,
  requestsPerMinute: 100,
};

// Affiliate
export const AMAZON_ASSOCIATE_TAG = "waitingthelon-20";
