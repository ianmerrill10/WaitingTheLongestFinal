/**
 * Shared types for the universal shelter scraper system.
 */

export interface ScrapedDog {
  external_id: string;
  name: string;
  breed_primary: string;
  breed_secondary?: string;
  breed_mixed?: boolean;
  age_text?: string;
  age_months?: number;
  gender?: "male" | "female";
  size?: "small" | "medium" | "large" | "xlarge";
  weight_lbs?: number;
  color_primary?: string;
  color_secondary?: string;
  description?: string;
  photo_urls: string[];
  primary_photo_url?: string;
  external_url?: string;
  intake_date?: string;
  euthanasia_date?: string;
  is_on_euthanasia_list?: boolean;
  is_spayed_neutered?: boolean;
  good_with_kids?: boolean;
  good_with_dogs?: boolean;
  good_with_cats?: boolean;
  house_trained?: boolean;
  has_special_needs?: boolean;
  special_needs_description?: string;
  adoption_fee?: number;
  tags?: string[];
  /**
   * IANA timezone of the shelter (e.g. "America/Los_Angeles").
   * When present, bare date strings like "2024-03-15" are parsed as noon local
   * time in this timezone instead of UTC midnight, eliminating off-by-one-day
   * errors for all US shelters.
   * When absent, the mapper defaults to "America/New_York" (Eastern).
   */
  shelter_timezone?: string;
}

export interface ScrapeResult {
  shelterId: string;
  shelterName: string;
  platform: string;
  dogs: ScrapedDog[];
  errors: string[];
  scrapedAt: string;
  duration: number;
}

export interface PlatformAdapter {
  name: string;
  detect(html: string, url: string): boolean;
  extractPlatformId(html: string, url: string): string | null;
  scrape(shelterUrl: string, platformId: string | null): Promise<ScrapedDog[]>;
}

export interface DetectionResult {
  platform: string;
  platformId: string | null;
  adoptablePageUrl: string | null;
}

export interface UpsertResult {
  inserted: number;
  updated: number;
  deactivated: number;
  errors: number;
}

export type ScrapeStatus =
  | "pending"
  | "success"
  | "partial"
  | "error"
  | "unreachable"
  | "no_dogs_page"
  | "skipped";

/**
 * Verification status of a shelter — mirrors shelters.verification_status in DB.
 * Determines the confidence_tier assigned to all child dogs via DB trigger.
 */
export type ShelterVerificationStatus =
  | "unverified"
  | "pending_review"
  | "verified_nonprofit"
  | "verified_government"
  | "rejected";

/**
 * Confidence tier assigned to a dog based on its parent shelter's verification.
 * Set automatically by the assign_dog_confidence_tier() DB trigger — scrapers
 * do not need to set this; it will be overwritten on INSERT anyway.
 * Exposed here for type-safe reads/filters in application code.
 */
export type DogConfidenceTier = "high" | "standard" | "low" | "hidden";

export type WebsitePlatform =
  | "shelterluv"
  | "rescuegroups"
  | "shelterbuddy"
  | "petango"
  | "petstablished"
  | "adoptapet"
  | "chameleon"
  | "sheltermanager"
  | "custom_html"
  | "facebook_only"
  | "no_website"
  | "unreachable"
  | "unknown";

export interface ShelterScrapeRecord {
  id: string;
  name: string;
  state_code: string;
  city: string;
  verification_status?: ShelterVerificationStatus;
  domain_verified?: boolean;
  website: string | null;
  website_platform: WebsitePlatform;
  adoptable_page_url: string | null;
  platform_shelter_id: string | null;
  last_scraped_at: string | null;
  scrape_status: ScrapeStatus;
  scrape_error: string | null;
  dogs_found_last_scrape: number;
  scrape_count: number;
  next_scrape_at: string | null;
  external_id: string | null;
  external_source: string | null;
}

export interface StateWorkerMetrics {
  stateCode: string;
  totalShelters: number;
  sheltersWithWebsite: number;
  sheltersScrapedToday: number;
  totalDogsFound: number;
  lastScrapeAt: string | null;
  errors: number;
  platforms: Record<string, number>;
}
