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
  description: string | null;
  is_kill_shelter: boolean;
  accepts_rescue_pulls: boolean;
  is_verified: boolean;
  is_active: boolean;
  total_dogs: number;
  available_dogs: number;
  critical_dogs: number;
  external_id: string | null;
  external_source: string | null;
  created_at: string;
  updated_at: string;
}

export interface ShelterStats {
  id: string;
  name: string;
  city: string;
  state_code: string;
  total_dogs: number;
  available_dogs: number;
  critical_dogs: number;
  high_urgency_dogs: number;
  avg_wait_days: number;
  max_wait_days: number;
}
