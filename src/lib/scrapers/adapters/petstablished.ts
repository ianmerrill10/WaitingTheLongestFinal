/**
 * Petstablished Adapter — scrapes petstablished.com widget pages.
 */
import * as cheerio from "cheerio";
import { fetchHTML } from "../rate-limiter";
import type { PlatformAdapter, ScrapedDog } from "../types";

export const petstablishedAdapter: PlatformAdapter = {
  name: "petstablished",

  detect(html: string): boolean {
    return /petstablished\.com/i.test(html);
  },

  extractPlatformId(html: string): string | null {
    const match = html.match(
      /petstablished\.com\/(?:pets|organization|embed)\/(\d+)/i
    );
    return match ? match[1] : null;
  },

  async scrape(shelterUrl: string, platformId: string | null): Promise<ScrapedDog[]> {
    const dogs: ScrapedDog[] = [];

    const url = platformId
      ? `https://petstablished.com/pets?organization_id=${platformId}&species=Dog`
      : shelterUrl;

    try {
      const html = await fetchHTML(url);
      const $ = cheerio.load(html);

      // Petstablished uses card-based layouts
      $(".pet-card, .animal-card, .card, [class*='pet-item']").each(
        (_, el) => {
          try {
            const card = $(el);

            const link =
              card.find("a[href*='/pet/']").attr("href") ||
              card.find("a[href*='/pets/']").attr("href") ||
              card.find("a").first().attr("href") ||
              "";

            const idMatch = link.match(/\/pets?\/(\d+)/i);
            const animalId = idMatch
              ? idMatch[1]
              : `ps-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;

            const name =
              card
                .find("h2, h3, h4, .pet-name, .name, [class*='name']")
                .first()
                .text()
                .trim() || card.find("a").first().text().trim();

            if (!name || name.length < 2) return;

            const breed =
              card
                .find(".breed, .pet-breed, [class*='breed']")
                .text()
                .trim() || "Mixed Breed";

            const ageText =
              card
                .find(".age, .pet-age, [class*='age']")
                .text()
                .trim() || undefined;

            const genderText =
              card
                .find(".sex, .gender, [class*='sex'], [class*='gender']")
                .text()
                .trim() || "";

            const photoUrl =
              card.find("img").first().attr("src") ||
              card.find("img").first().attr("data-src") ||
              "";

            const detailUrl = link.startsWith("http")
              ? link
              : `https://petstablished.com${link}`;

            dogs.push({
              external_id: `petstablished-${platformId || "unknown"}-${animalId}`,
              name,
              breed_primary: breed.split("/")[0].trim(),
              breed_mixed: breed.toLowerCase().includes("mix"),
              age_text: ageText,
              gender: genderText.toLowerCase().includes("female")
                ? "female"
                : "male",
              photo_urls: photoUrl ? [photoUrl] : [],
              primary_photo_url: photoUrl || undefined,
              external_url: detailUrl,
              tags: ["petstablished"],
            });
          } catch {
            // Skip malformed cards
          }
        }
      );
    } catch {
      // Fetch failed
    }

    return dogs;
  },
};
