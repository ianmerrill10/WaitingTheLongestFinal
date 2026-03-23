/**
 * Website Discovery — finds websites for IRS-only shelters that have no URL.
 * Uses web search heuristics to discover shelter websites.
 *
 * Run as part of the scraper agent (low priority, background task).
 */
import { createAdminClient } from "@/lib/supabase/admin";
import { fetchHTML } from "./rate-limiter";

/**
 * Attempt to discover a website for a shelter with no URL.
 * Tries common URL patterns and validates them.
 */
export async function discoverWebsite(
  shelterName: string,
  city: string,
  state: string
): Promise<string | null> {
  // Generate candidate URLs from the shelter name
  const cleanName = shelterName
    .toLowerCase()
    .replace(/[^a-z0-9\s]/g, "")
    .replace(/\s+/g, "-")
    .replace(/-+/g, "-")
    .substring(0, 40);

  const candidates = [
    `https://www.${cleanName}.org`,
    `https://www.${cleanName}.com`,
    `https://${cleanName}.org`,
    `https://${cleanName}.com`,
  ];

  for (const url of candidates) {
    try {
      const html = await fetchHTML(url);
      if (!html) continue;

      // Validate: does this look like an animal rescue site?
      const lower = html.toLowerCase();
      const animalKeywords = [
        "adopt", "rescue", "shelter", "animal",
        "dog", "cat", "pet", "foster", "spay", "neuter",
      ];
      const matches = animalKeywords.filter((kw) => lower.includes(kw));
      if (matches.length >= 3) {
        // Check if the page mentions the city/state
        if (lower.includes(city.toLowerCase()) || lower.includes(state.toLowerCase())) {
          return url;
        }
        // Even without city match, 4+ keywords is strong signal
        if (matches.length >= 4) {
          return url;
        }
      }
    } catch {
      continue;
    }
  }

  return null;
}

/**
 * Process a batch of shelters without websites.
 * Returns count of websites discovered.
 */
export async function discoverWebsiteBatch(limit: number = 50): Promise<number> {
  const supabase = createAdminClient();

  const { data: shelters } = await supabase
    .from("shelters")
    .select("id, name, city, state_code")
    .or("website.is.null,website.eq.")
    .limit(limit);

  if (!shelters || shelters.length === 0) return 0;

  let found = 0;

  for (const shelter of shelters) {
    try {
      const url = await discoverWebsite(
        shelter.name,
        shelter.city || "",
        shelter.state_code
      );

      if (url) {
        await supabase
          .from("shelters")
          .update({
            website: url,
            website_platform: "unknown", // Queue for classification
            scrape_status: "pending",
          })
          .eq("id", shelter.id);

        found++;
        console.log(`  Discovered: ${shelter.name} → ${url}`);
      }
    } catch {
      // Skip errors
    }
  }

  return found;
}
