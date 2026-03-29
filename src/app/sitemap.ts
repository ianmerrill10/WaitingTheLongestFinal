import { MetadataRoute } from "next";
import { createAdminClient } from "@/lib/supabase/admin";

const BASE_URL = "https://waitingthelongest.com";

const STATE_CODES = [
  "al", "ak", "az", "ar", "ca", "co", "ct", "de", "fl", "ga",
  "hi", "id", "il", "in", "ia", "ks", "ky", "la", "me", "md",
  "ma", "mi", "mn", "ms", "mo", "mt", "ne", "nv", "nh", "nj",
  "nm", "ny", "nc", "nd", "oh", "ok", "or", "pa", "ri", "sc",
  "sd", "tn", "tx", "ut", "vt", "va", "wa", "wv", "wi", "wy",
];

export default async function sitemap(): Promise<MetadataRoute.Sitemap> {
  // Static pages with their change frequencies and priorities
  const staticPages: MetadataRoute.Sitemap = [
    {
      url: BASE_URL,
      lastModified: new Date(),
      changeFrequency: "daily",
      priority: 1.0,
    },
    {
      url: `${BASE_URL}/dogs`,
      lastModified: new Date(),
      changeFrequency: "hourly",
      priority: 0.9,
    },
    {
      url: `${BASE_URL}/urgent`,
      lastModified: new Date(),
      changeFrequency: "hourly",
      priority: 0.95,
    },
    {
      url: `${BASE_URL}/overlooked`,
      lastModified: new Date(),
      changeFrequency: "daily",
      priority: 0.8,
    },
    {
      url: `${BASE_URL}/states`,
      lastModified: new Date(),
      changeFrequency: "weekly",
      priority: 0.7,
    },
    {
      url: `${BASE_URL}/shelters`,
      lastModified: new Date(),
      changeFrequency: "daily",
      priority: 0.8,
    },
    {
      url: `${BASE_URL}/search`,
      lastModified: new Date(),
      changeFrequency: "daily",
      priority: 0.7,
    },
    {
      url: `${BASE_URL}/about`,
      lastModified: new Date(),
      changeFrequency: "monthly",
      priority: 0.5,
    },
    {
      url: `${BASE_URL}/foster`,
      lastModified: new Date(),
      changeFrequency: "weekly",
      priority: 0.6,
    },
  ];

  // State pages
  const statePages: MetadataRoute.Sitemap = STATE_CODES.map((code) => ({
    url: `${BASE_URL}/states/${code}`,
    lastModified: new Date(),
    changeFrequency: "daily" as const,
    priority: 0.7,
  }));

  // Legal pages
  const legalPages: MetadataRoute.Sitemap = [
    "terms-of-service", "privacy-policy", "dmca", "shelter-terms", "data-sharing", "api-terms",
  ].map((slug) => ({
    url: `${BASE_URL}/legal/${slug}`,
    lastModified: new Date(),
    changeFrequency: "monthly" as const,
    priority: 0.3,
  }));

  // Dynamic pages from database
  let dogPages: MetadataRoute.Sitemap = [];
  let shelterPages: MetadataRoute.Sitemap = [];
  let breedPages: MetadataRoute.Sitemap = [];

  try {
    const supabase = createAdminClient();

    // Fetch top 5000 available dogs ordered by intake_date (longest waiting first)
    const { data: dogs } = await supabase
      .from("dogs")
      .select("id, updated_at")
      .eq("is_available", true)
      .order("intake_date", { ascending: true })
      .limit(5000);

    if (dogs) {
      dogPages = dogs.map((dog) => ({
        url: `${BASE_URL}/dogs/${dog.id}`,
        lastModified: new Date(dog.updated_at),
        changeFrequency: "daily" as const,
        priority: 0.6,
      }));
    }

    // Fetch top 1000 shelters ordered by available_dogs count
    const { data: shelters } = await supabase
      .from("shelters")
      .select("id, updated_at")
      .eq("is_active", true)
      .order("available_dogs", { ascending: false })
      .limit(1000);

    if (shelters) {
      shelterPages = shelters.map((shelter) => ({
        url: `${BASE_URL}/shelters/${shelter.id}`,
        lastModified: new Date(shelter.updated_at),
        changeFrequency: "daily" as const,
        priority: 0.6,
      }));
    }

    // Fetch distinct breeds for breed pages
    const { data: breeds } = await supabase
      .from("dogs")
      .select("breed_primary")
      .eq("is_available", true)
      .not("breed_primary", "is", null)
      .limit(1000);

    if (breeds) {
      const uniqueBreeds = Array.from(new Set(breeds.map((b) => b.breed_primary).filter(Boolean)));
      breedPages = uniqueBreeds.map((breed) => ({
        url: `${BASE_URL}/breeds/${encodeURIComponent(breed.toLowerCase().replace(/\s+/g, "-"))}`,
        lastModified: new Date(),
        changeFrequency: "weekly" as const,
        priority: 0.5,
      }));
    }
  } catch {
    // If the database is unavailable, we still return static and state routes
  }

  return [...staticPages, ...statePages, ...legalPages, ...dogPages, ...shelterPages, ...breedPages];
}
