/**
 * ShelterBuddy Adapter — scrapes ShelterBuddy public API and HTML pages.
 * Based on the existing NYC ACC scraper pattern.
 * ShelterBuddy has a public /api/animals endpoint that returns JSON.
 */
import * as cheerio from "cheerio";
import { fetchHTML, fetchJSON } from "../rate-limiter";
import type { PlatformAdapter, ScrapedDog } from "../types";
import { parseAgeToMonths } from "../dog-mapper";

function normalizeShelterBuddySize(size?: string): ScrapedDog["size"] | undefined {
  if (!size) return undefined;
  const s = size.toLowerCase().trim();
  if (s.includes("extra") || s.includes("xlarge") || s.includes("x-large") || s.includes("giant")) return "xlarge";
  if (s.includes("small") || s.includes("toy") || s.includes("mini")) return "small";
  if (s.includes("large")) return "large";
  if (s.includes("medium") || s.includes("med")) return "medium";
  return undefined;
}

interface ShelterBuddyAnimal {
  Id: number;
  ShelterBuddyId?: number;
  Name?: string;
  Breed?: string;
  Sex?: string;
  Color?: string;
  Age?: string;
  Weight?: string;
  Description?: string;
  AdoptionSummary?: string;
  AvailableForAdoptionFrom?: string;
  // Potential euthanasia/commitment date fields (vary by shelter config)
  CommitmentDate?: string;
  DateToBeDestroyed?: string;
  DueOutDate?: string;
  OutcomeDate?: string;
  Photos?: string[];
  Photo?: string;
  Species?: string;
  IsDesexed?: boolean;
  Size?: string;
  GoodWithKids?: boolean;
  GoodWithDogs?: boolean;
  GoodWithCats?: boolean;
  HouseTrained?: boolean;
  SpecialNeeds?: boolean;
  SpecialNeedsDescription?: string;
}

export const shelterbuddyAdapter: PlatformAdapter = {
  name: "shelterbuddy",

  detect(html: string): boolean {
    return /shelterbuddy\.com/i.test(html) || /ShelterBuddy/i.test(html);
  },

  extractPlatformId(html: string): string | null {
    const match = html.match(
      /https?:\/\/([^/.]+)\.shelterbuddy\.com/i
    );
    return match ? match[1] : null;
  },

  async scrape(shelterUrl: string, platformId: string | null): Promise<ScrapedDog[]> {
    const dogs: ScrapedDog[] = [];

    // Strategy 1: Try public API endpoint
    if (platformId) {
      try {
        const apiUrl = `https://${platformId}.shelterbuddy.com/api/animals`;
        const animals = await fetchJSON<ShelterBuddyAnimal[]>(apiUrl);

        if (Array.isArray(animals)) {
          for (const animal of animals) {
            // Filter to dogs only
            if (
              animal.Species &&
              !animal.Species.toLowerCase().includes("dog") &&
              !animal.Species.toLowerCase().includes("canine")
            )
              continue;

            const photos: string[] = [];
            if (animal.Photos) photos.push(...animal.Photos);
            else if (animal.Photo) photos.push(animal.Photo);

            // AvailableForAdoptionFrom is GOLD — real intake date
            let intakeDate: string | undefined;
            if (animal.AvailableForAdoptionFrom) {
              const d = new Date(animal.AvailableForAdoptionFrom);
              if (!isNaN(d.getTime())) {
                intakeDate = d.toISOString();
              }
            }

            // Check for euthanasia/commitment date
            let euthanasiaDate: string | undefined;
            const commitDate = animal.CommitmentDate || animal.DateToBeDestroyed || animal.DueOutDate;
            if (commitDate) {
              const d = new Date(commitDate);
              if (!isNaN(d.getTime()) && d > new Date()) {
                euthanasiaDate = d.toISOString();
              }
            }

            // Parse weight
            let weightLbs: number | undefined;
            if (animal.Weight) {
              const wMatch = animal.Weight.match(/([\d.]+)/);
              if (wMatch) weightLbs = parseFloat(wMatch[1]);
            }

            dogs.push({
              external_id: `shelterbuddy-${platformId}-${animal.ShelterBuddyId || animal.Id}`,
              name: animal.Name || "Unknown",
              breed_primary: animal.Breed?.split("/")[0].trim() || "Mixed Breed",
              breed_secondary: animal.Breed?.includes("/")
                ? animal.Breed.split("/")[1].trim()
                : undefined,
              age_text: animal.Age,
              age_months: parseAgeToMonths(animal.Age) ?? undefined,
              gender: animal.Sex?.toLowerCase().includes("female")
                ? "female"
                : "male",
              size: normalizeShelterBuddySize(animal.Size),
              weight_lbs: weightLbs,
              color_primary: animal.Color,
              description: animal.Description || animal.AdoptionSummary,
              photo_urls: photos,
              primary_photo_url: photos[0],
              intake_date: intakeDate,
              euthanasia_date: euthanasiaDate,
              is_on_euthanasia_list: !!euthanasiaDate,
              is_spayed_neutered: animal.IsDesexed,
              good_with_kids: animal.GoodWithKids,
              good_with_dogs: animal.GoodWithDogs,
              good_with_cats: animal.GoodWithCats,
              house_trained: animal.HouseTrained,
              has_special_needs: animal.SpecialNeeds,
              special_needs_description: animal.SpecialNeedsDescription,
              external_url: `https://${platformId}.shelterbuddy.com/animal/animalDetails.asp?task=view&animalid=${animal.ShelterBuddyId || animal.Id}`,
              tags: ["shelterbuddy"],
            });
          }

          return dogs;
        }
      } catch {
        // API not available — fall through to HTML scraping
      }
    }

    // Strategy 2: HTML scraping (same pattern as NYC ACC)
    try {
      const url = platformId
        ? `https://${platformId}.shelterbuddy.com/search/searchResults.asp?task=search&searchType=4&speciesID=1`
        : shelterUrl;

      const html = await fetchHTML(url);
      const $ = cheerio.load(html);

      // ShelterBuddy search results typically use table or card layout
      $("table tr, .animal-result, .search-result").each((_, el) => {
        try {
          const row = $(el);
          const link = row.find("a[href*='animalDetails']").attr("href");
          if (!link) return;

          const idMatch = link.match(/animalid=(\d+)/i);
          if (!idMatch) return;

          const animalId = idMatch[1];
          const name = row.find("a[href*='animalDetails']").first().text().trim();
          if (!name) return;

          const cells = row.find("td");
          const breed = cells.eq(1).text().trim() || "Mixed Breed";
          const sex = cells.eq(2).text().trim();
          const age = cells.eq(3).text().trim();
          const photo = row.find("img").first().attr("src");

          const host = platformId
            ? `https://${platformId}.shelterbuddy.com`
            : new URL(shelterUrl).origin;

          dogs.push({
            external_id: `shelterbuddy-${platformId || "unknown"}-${animalId}`,
            name,
            breed_primary: breed.split("/")[0].trim(),
            age_text: age || undefined,
            gender: sex.toLowerCase().includes("female") ? "female" : "male",
            photo_urls: photo ? [photo.startsWith("http") ? photo : `${host}${photo}`] : [],
            external_url: `${host}/animal/animalDetails.asp?task=view&animalid=${animalId}`,
            tags: ["shelterbuddy"],
          });
        } catch {
          // Skip malformed rows
        }
      });
    } catch {
      // HTML scraping failed
    }

    return dogs;
  },
};
