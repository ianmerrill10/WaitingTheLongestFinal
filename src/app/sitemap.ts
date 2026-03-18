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

  // Dynamic pages from database
  let dogPages: MetadataRoute.Sitemap = [];
  let shelterPages: MetadataRoute.Sitemap = [];

  try {
    const supabase = createAdminClient();

    // Fetch top 100 available dogs ordered by intake_date (longest waiting first)
    const { data: dogs } = await supabase
      .from("dogs")
      .select("id, updated_at")
      .eq("is_available", true)
      .order("intake_date", { ascending: true })
      .limit(100);

    if (dogs) {
      dogPages = dogs.map((dog) => ({
        url: `${BASE_URL}/dogs/${dog.id}`,
        lastModified: new Date(dog.updated_at),
        changeFrequency: "daily" as const,
        priority: 0.6,
      }));
    }

    // Fetch top 100 shelters ordered by available_dogs count
    const { data: shelters } = await supabase
      .from("shelters")
      .select("id, updated_at")
      .eq("is_active", true)
      .order("available_dogs", { ascending: false })
      .limit(100);

    if (shelters) {
      shelterPages = shelters.map((shelter) => ({
        url: `${BASE_URL}/shelters/${shelter.id}`,
        lastModified: new Date(shelter.updated_at),
        changeFrequency: "daily" as const,
        priority: 0.6,
      }));
    }
  } catch (error) {
    // If the database is unavailable, we still return static and state routes
    console.error("Sitemap: Failed to fetch dynamic routes from database:", error);
  }

  return [...staticPages, ...statePages, ...dogPages, ...shelterPages];
}
