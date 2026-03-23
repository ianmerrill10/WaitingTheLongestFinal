/**
 * RescueGroups Widget Adapter — extracts org ID from embedded toolkit
 * and fetches data via the existing RescueGroups API client.
 */
import { createRescueGroupsClient } from "@/lib/rescuegroups/client";
import { extractPhotoUrls } from "@/lib/rescuegroups/mapper";
import type { PlatformAdapter, ScrapedDog } from "../types";

export const rescuegroupsWidgetAdapter: PlatformAdapter = {
  name: "rescuegroups",

  detect(html: string): boolean {
    return (
      /toolkit\.rescuegroups\.org/i.test(html) ||
      /rescuegroups\.org\/toolkit/i.test(html) ||
      /data-rg-/i.test(html)
    );
  },

  extractPlatformId(html: string): string | null {
    const match =
      html.match(/orgID["':\s]+["']?(\d+)/i) ||
      html.match(/rescuegroups\.org[^"']*orgID=(\d+)/i) ||
      html.match(/data-rg-org-?id=["']?(\d+)/i);
    return match ? match[1] : null;
  },

  async scrape(_shelterUrl: string, platformId: string | null): Promise<ScrapedDog[]> {
    if (!platformId) return [];

    const dogs: ScrapedDog[] = [];
    const client = createRescueGroupsClient();

    try {
      // Use the RG API to fetch animals for this org
      // The search endpoint supports org filtering
      const response = await client.fetchAnimals(1, 100);

      if (!response.data) return [];

      const included = response.included || [];

      for (const animal of response.data) {
        // Filter to this org's animals
        const orgId = animal.relationships?.orgs?.data?.[0]?.id;
        if (orgId !== platformId) continue;

        const attrs = animal.attributes;
        if (!attrs) continue;

        // Extract photos
        const pictureIds =
          animal.relationships?.pictures?.data?.map(
            (p: { id: string }) => p.id
          ) || [];
        const photoData = extractPhotoUrls(pictureIds, included);

        dogs.push({
          external_id: `rg-widget-${platformId}-${animal.id}`,
          name: attrs.name || "Unknown",
          breed_primary: attrs.breedPrimary || "Mixed Breed",
          breed_secondary: attrs.breedSecondary || undefined,
          breed_mixed: (attrs as unknown as Record<string, unknown>).isMixedBreed as boolean || false,
          age_text: attrs.ageString || undefined,
          gender:
            attrs.sex?.toLowerCase() === "female" ? "female" : "male",
          size: (attrs.sizeGroup?.toLowerCase() as "small" | "medium" | "large" | "xlarge") || undefined,
          color_primary: (attrs as unknown as Record<string, unknown>).colorDetails as string || undefined,
          description: attrs.descriptionText || undefined,
          photo_urls: photoData.photos,
          primary_photo_url: photoData.primary || undefined,
          external_url: `https://www.rescuegroups.org/animals/${animal.id}`,
          is_spayed_neutered: attrs.isAltered || false,
          tags: ["rescuegroups-widget"],
        });
      }
    } catch {
      // RG API error — return empty
    }

    return dogs;
  },
};
