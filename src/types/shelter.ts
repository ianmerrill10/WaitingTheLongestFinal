export interface Shelter {
  id: string;
  name: string;
  slug: string;
  shelter_type: "municipal" | "private" | "rescue" | "foster_network" | "humane_society" | "spca";
  address: string | null;
  city: string;
  state_code: string;
  zip_code: string | null;
  county: string | null;
  latitude: number | null;
  longitude: number | null;
  phone: string | null;
  email: string | null;
  website: string | null;
  facebook_url: string | null;
  description: string | null;
  is_kill_shelter: boolean;
  urgency_multiplier: number | null;
  accepts_rescue_pulls: boolean;
  is_verified: boolean;
  is_active: boolean;
  ein: string | null;
  external_id: string | null;
  external_source: string | null;
  total_animals: number;
  available_dog_count: number;
  urgent_dog_count: number;
  website_platform: string | null;
  adoptable_page_url: string | null;
  stray_hold_days: number | null;
  last_synced_at: string | null;
  last_scraped_at: string | null;
  created_at: string;
  updated_at: string;
}

export interface ShelterStats {
  id: string;
  name: string;
  city: string;
  state_code: string;
  total_animals: number;
  available_dog_count: number;
  urgent_dog_count: number;
  avg_wait_days: number;
  max_wait_days: number;
}
