import type { UrgencyLevel } from "@/lib/constants";

export interface Dog {
  id: string;
  name: string;
  shelter_id: string;
  breed_primary: string;
  breed_secondary: string | null;
  breed_mixed: boolean;
  size: "small" | "medium" | "large" | "xlarge";
  age_category: "puppy" | "young" | "adult" | "senior";
  age_months: number | null;
  gender: "male" | "female";
  color_primary: string | null;
  color_secondary: string | null;
  weight_lbs: number | null;
  is_spayed_neutered: boolean;
  status: string;
  is_available: boolean;
  intake_date: string;
  date_confidence: "verified" | "high" | "medium" | "low" | "unknown" | null;
  date_source: string | null;
  last_audited_at: string | null;
  intake_type: string | null;
  available_date: string | null;
  euthanasia_date: string | null;
  urgency_level: UrgencyLevel;
  is_on_euthanasia_list: boolean;
  euthanasia_list_added_at: string | null;
  rescue_pull_status: string | null;
  photo_urls: string[];
  primary_photo_url: string | null;
  video_url: string | null;
  thumbnail_url: string | null;
  description: string | null;
  short_description: string | null;
  tags: string[];
  good_with_kids: boolean | null;
  good_with_dogs: boolean | null;
  good_with_cats: boolean | null;
  house_trained: boolean | null;
  has_special_needs: boolean;
  special_needs_description: string | null;
  has_medical_conditions: boolean;
  is_heartworm_positive: boolean;
  energy_level: "low" | "medium" | "high" | null;
  adoption_fee: number | null;
  is_fee_waived: boolean;
  external_id: string | null;
  external_source: string | null;
  external_url: string | null;
  view_count: number;
  favorite_count: number;
  share_count: number;
  created_at: string;
  updated_at: string;
  // Joined fields
  shelter_name?: string;
  shelter_city?: string;
  shelter_state?: string;
}

export interface DogWithWaitTime extends Dog {
  wait_days_total: number;
  wait_years: number;
  wait_months: number;
  wait_days: number;
  wait_hours: number;
  wait_minutes: number;
  wait_seconds: number;
  wait_time_formatted: string;
}

export interface UrgentDog extends Dog {
  countdown_days: number | null;
  countdown_hours: number | null;
  countdown_minutes: number | null;
  countdown_seconds: number | null;
  countdown_formatted: string | null;
  countdown_hours_total: number | null;
  is_critical: boolean;
  calculated_urgency: string;
}

export interface DogFilters {
  breed?: string;
  size?: string;
  age?: string;
  gender?: string;
  state?: string;
  urgency?: UrgencyLevel;
  good_with_kids?: boolean;
  good_with_dogs?: boolean;
  good_with_cats?: boolean;
  has_special_needs?: boolean;
  search?: string;
  sort?: "wait_time" | "urgency" | "newest" | "name";
  page?: number;
}
