/**
 * ShelterLuv Adapter — scrapes shelterluv.com public available_animals pages.
 * ShelterLuv is one of the most common shelter management platforms.
 */
import * as cheerio from "cheerio";
import { fetchHTML } from "../rate-limiter";
import type { PlatformAdapter, ScrapedDog } from "../types";

export const shelterluvAdapter: PlatformAdapter = {
  name: "shelterluv",

  detect(html: string): boolean {
    return /shelterluv\.com/i.test(html) || /cdn\.shelterluv\.com/i.test(html);
  },

  extractPlatformId(html: string): string | null {
    const match =
      html.match(/shelterluv\.com\/(?:available_animals|embed)\/?(\d+)/i) ||
      html.match(/data-org-?id=["']?(\d+)/i);
    return match ? match[1] : null;
  },

  async scrape(shelterUrl: string, platformId: string | null): Promise<ScrapedDog[]> {
    if (!platformId) return [];

    const dogs: ScrapedDog[] = [];
    const baseUrl = `https://www.shelterluv.com/available_animals/${platformId}`;

    try {
      // Fetch the available animals page filtered to dogs
      const html = await fetchHTML(`${baseUrl}?species=Dog`);
      const $ = cheerio.load(html);

      // ShelterLuv uses various card layouts — try multiple selectors
      const animalCards = $(
        '.animal-card, .pet-card, [class*="animal"], .card, .pet-listing'
      );

      animalCards.each((_, el) => {
        try {
          const card = $(el);

          // Extract animal ID from links
          const detailLink =
            card.find("a[href*='/animal/']").attr("href") ||
            card.find("a[href*='/pet/']").attr("href") ||
            card.find("a").first().attr("href") ||
            "";

          const idMatch = detailLink.match(/\/(?:animal|pet)\/(\d+)/);
          if (!idMatch) return;

          const animalId = idMatch[1];

          // Extract name
          const name =
            card.find("h2, h3, h4, .name, .pet-name, .animal-name").first().text().trim() ||
            card.find("a").first().text().trim();

          if (!name) return;

          // Extract breed
          const breed =
            card.find('.breed, .pet-breed, [class*="breed"]').text().trim() ||
            "Mixed Breed";

          // Extract age
          const ageText =
            card.find('.age, .pet-age, [class*="age"]').text().trim() || undefined;

          // Extract sex/gender
          const genderText =
            card.find('.sex, .gender, .pet-sex, [class*="sex"], [class*="gender"]').text().trim();

          // Extract photo
          const photoUrl =
            card.find("img").first().attr("src") ||
            card.find("img").first().attr("data-src") ||
            "";

          // Build detail URL
          const detailUrl = detailLink.startsWith("http")
            ? detailLink
            : `https://www.shelterluv.com${detailLink}`;

          dogs.push({
            external_id: `shelterluv-${platformId}-${animalId}`,
            name,
            breed_primary: breed.split("/")[0].trim() || "Mixed Breed",
            breed_secondary: breed.includes("/")
              ? breed.split("/")[1].trim()
              : undefined,
            breed_mixed: breed.toLowerCase().includes("mix"),
            age_text: ageText,
            gender: genderText?.toLowerCase().includes("female")
              ? "female"
              : "male",
            photo_urls: photoUrl ? [photoUrl] : [],
            primary_photo_url: photoUrl || undefined,
            external_url: detailUrl,
            tags: ["shelterluv"],
          });
        } catch {
          // Skip malformed cards
        }
      });

      // Fallback: try parsing as JSON (some ShelterLuv pages return JSON data)
      if (dogs.length === 0) {
        const jsonMatch = html.match(
          /window\.__INITIAL_(?:STATE|DATA)__\s*=\s*({[\s\S]*?});/
        );
        if (jsonMatch) {
          try {
            const data = JSON.parse(jsonMatch[1]);
            const animals =
              data.animals || data.pets || data.available || [];
            for (const animal of animals) {
              if (
                animal.Species &&
                animal.Species.toLowerCase() !== "dog"
              )
                continue;
              dogs.push({
                external_id: `shelterluv-${platformId}-${animal.ID || animal.AnimalID || animal.id}`,
                name: animal.Name || animal.name || "Unknown",
                breed_primary:
                  animal.Breed || animal.breed || "Mixed Breed",
                age_text: animal.Age || animal.age,
                gender: (animal.Sex || animal.sex || "")
                  .toLowerCase()
                  .includes("female")
                  ? "female"
                  : "male",
                size: animal.Size || animal.size,
                weight_lbs: parseFloat(animal.Weight || animal.weight) || undefined,
                description: animal.Description || animal.description,
                photo_urls: animal.Photos || animal.photos || [],
                primary_photo_url:
                  animal.CoverPhoto || animal.Photo || animal.photo,
                external_url: `https://www.shelterluv.com/animal/${animal.ID || animal.AnimalID || animal.id}`,
                tags: ["shelterluv"],
              });
            }
          } catch {
            // JSON parse failed — ignore
          }
        }
      }
    } catch {
      // Fetch failed
    }

    return dogs;
  },
};
