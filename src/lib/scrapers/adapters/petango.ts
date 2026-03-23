/**
 * Petango / PetPoint Adapter — scrapes petango.com adoptable pet listings.
 * PetPoint shelters expose animals on petango.com or service.petango.com.
 */
import * as cheerio from "cheerio";
import { fetchHTML } from "../rate-limiter";
import type { PlatformAdapter, ScrapedDog } from "../types";
import { parseAgeToMonths } from "../dog-mapper";

export const petangoAdapter: PlatformAdapter = {
  name: "petango",

  detect(html: string): boolean {
    return (
      /petango\.com/i.test(html) ||
      /service\.petango\.com/i.test(html) ||
      /petpoint/i.test(html)
    );
  },

  extractPlatformId(html: string): string | null {
    const match =
      html.match(/petango\.com[^"']*[?&](?:shelterId|sId)=([^"'&]+)/i) ||
      html.match(/CompanyID=([^"'&]+)/i);
    return match ? match[1] : null;
  },

  async scrape(shelterUrl: string, platformId: string | null): Promise<ScrapedDog[]> {
    const dogs: ScrapedDog[] = [];

    const url = platformId
      ? `https://www.petango.com/sms/adoptable/pet?species=Dog&sex=A&agegroup=All&location=All&site=&onhold=A&orderby=ID&colession=${platformId}`
      : shelterUrl;

    try {
      const html = await fetchHTML(url);
      const $ = cheerio.load(html);

      // Petango uses a list/grid of animal cards
      $(
        ".list-animal-card, .animal-card, .list-item, .pet-item, tr[class*='list']"
      ).each((_, el) => {
        try {
          const card = $(el);

          // Find detail link
          const detailLink =
            card.find("a[href*='pet']").attr("href") ||
            card.find("a").first().attr("href") ||
            "";

          const idMatch =
            detailLink.match(/[?&]id=(\d+)/i) ||
            detailLink.match(/\/pet\/(\d+)/i);

          const animalId = idMatch ? idMatch[1] : `petango-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`;

          // Extract fields
          const name =
            card.find(".list-animal-name, .pet-name, h3, h4").first().text().trim() ||
            card.find("a").first().text().trim();

          if (!name || name.length < 2) return;

          const breed =
            card
              .find(".list-animal-breed, .pet-breed, [class*='breed']")
              .text()
              .trim() || "Mixed Breed";

          const ageText =
            card
              .find(".list-animal-age, .pet-age, [class*='age']")
              .text()
              .trim() || undefined;

          const sexText =
            card
              .find(".list-animal-sex, .pet-sex, [class*='sex']")
              .text()
              .trim() || "";

          const photoUrl =
            card.find("img").first().attr("src") ||
            card.find("img").first().attr("data-src") ||
            "";

          const fullDetailUrl = detailLink.startsWith("http")
            ? detailLink
            : `https://www.petango.com${detailLink}`;

          dogs.push({
            external_id: `petango-${platformId || "unknown"}-${animalId}`,
            name,
            breed_primary: breed.split("/")[0].trim(),
            breed_secondary: breed.includes("/")
              ? breed.split("/")[1].trim()
              : undefined,
            age_text: ageText,
            age_months: parseAgeToMonths(ageText) ?? undefined,
            gender: sexText.toLowerCase().includes("female")
              ? "female"
              : "male",
            photo_urls: photoUrl ? [photoUrl] : [],
            primary_photo_url: photoUrl || undefined,
            external_url: fullDetailUrl,
            tags: ["petango", "petpoint"],
          });
        } catch {
          // Skip malformed cards
        }
      });
    } catch {
      // Fetch failed
    }

    return dogs;
  },
};
