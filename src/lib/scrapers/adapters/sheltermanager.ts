/**
 * ShelterManager (ASM) Adapter
 * Animal Shelter Manager is open-source software with a public JSON API.
 * API endpoint: https://{service}.sheltermanager.com/service?method=json_adoptable_animals&account={id}
 */
import { fetchJSON, fetchHTML } from "../rate-limiter";
import type { PlatformAdapter, ScrapedDog } from "../types";
import { parseAgeToMonths } from "../dog-mapper";

interface ASMAnimal {
  ID: number;
  ANIMALNAME: string;
  BREEDNAME?: string;
  BREED2NAME?: string;
  CROSSBREED?: number;
  SPECIESNAME?: string;
  SEXNAME?: string;
  SIZENAME?: string;
  BASECOLOURNAME?: string;
  AGEGROUP?: string;
  DATEOFBIRTH?: string;
  MOSTRECENTENTRYDATE?: string;
  ANIMALCOMMENTS?: string;
  WEBSITEMEDIANOTES?: string;
  WEBSITEMEDIANAME?: string;
  MEDIANOTES?: string;
  HASACTIVERESERVE?: number;
  ISGOODWITHCATS?: number;
  ISGOODWITHDOGS?: number;
  ISGOODWITHCHILDREN?: number;
  ISHOUSETRAINED?: number;
  NEUTERED?: number;
  HASSPECIALNEEDS?: number;
  HEALTHPROBLEMS?: string;
  WEIGHT?: number;
  SHELTERCODE?: string;
  SHORTCODE?: string;
  WEBSITEIMAGECOUNT?: number;
}

export const sheltermanagerAdapter: PlatformAdapter = {
  name: "sheltermanager",

  detect(html: string, url: string): boolean {
    return /sheltermanager\.com/i.test(html) || /sheltermanager\.com/i.test(url) || /asm\.?online/i.test(html);
  },

  extractPlatformId(html: string, url: string): string | null {
    // Look for the account/service name
    const match = html.match(/https?:\/\/([^/.]+)\.sheltermanager\.com/i)
      || url.match(/https?:\/\/([^/.]+)\.sheltermanager\.com/i)
      || html.match(/account=([a-z0-9]+)/i);
    return match ? match[1] : null;
  },

  async scrape(shelterUrl: string, platformId: string | null): Promise<ScrapedDog[]> {
    const dogs: ScrapedDog[] = [];

    if (!platformId) {
      // Try to extract from the shelter's page
      try {
        const html = await fetchHTML(shelterUrl);
        const match = html.match(/https?:\/\/([^/.]+)\.sheltermanager\.com/i)
          || html.match(/account=([a-z0-9]+)/i);
        if (match) platformId = match[1];
      } catch { /* skip */ }
    }

    if (!platformId) return dogs;

    try {
      // Fetch adoptable animals via JSON API
      const apiUrl = `https://service.sheltermanager.com/asmservice?account=${platformId}&method=json_adoptable_animals`;
      const animals = await fetchJSON<ASMAnimal[]>(apiUrl);

      if (!Array.isArray(animals)) return dogs;

      for (const animal of animals) {
        // Filter to dogs only
        if (animal.SPECIESNAME && !animal.SPECIESNAME.toLowerCase().includes("dog")) continue;

        // Parse intake date from MOSTRECENTENTRYDATE
        let intakeDate: string | undefined;
        if (animal.MOSTRECENTENTRYDATE) {
          const d = new Date(animal.MOSTRECENTENTRYDATE);
          if (!isNaN(d.getTime())) intakeDate = d.toISOString();
        }

        // Parse date of birth for age
        let ageMonths: number | undefined;
        if (animal.DATEOFBIRTH) {
          const dob = new Date(animal.DATEOFBIRTH);
          if (!isNaN(dob.getTime())) {
            ageMonths = Math.floor((Date.now() - dob.getTime()) / (1000 * 60 * 60 * 24 * 30.44));
          }
        }

        // Map size
        let size: ScrapedDog["size"];
        if (animal.SIZENAME) {
          const s = animal.SIZENAME.toLowerCase();
          if (s.includes("small")) size = "small";
          else if (s.includes("medium")) size = "medium";
          else if (s.includes("large") && !s.includes("extra")) size = "large";
          else if (s.includes("extra") || s.includes("giant")) size = "xlarge";
        }

        // Photos — ASM stores media with predictable URLs
        const photoUrls: string[] = [];
        const imageCount = animal.WEBSITEIMAGECOUNT || 0;
        for (let i = 1; i <= Math.min(imageCount, 5); i++) {
          photoUrls.push(`https://service.sheltermanager.com/asmservice?account=${platformId}&method=animal_image&animalid=${animal.ID}&seq=${i}`);
        }
        // Always try primary photo
        if (photoUrls.length === 0) {
          photoUrls.push(`https://service.sheltermanager.com/asmservice?account=${platformId}&method=animal_image&animalid=${animal.ID}`);
        }

        // Weight in kg from ASM, convert to lbs
        let weightLbs: number | undefined;
        if (animal.WEIGHT && animal.WEIGHT > 0) {
          weightLbs = Math.round(animal.WEIGHT * 2.205);
        }

        dogs.push({
          external_id: `asm-${platformId}-${animal.ID}`,
          name: animal.ANIMALNAME || "Unknown",
          breed_primary: animal.BREEDNAME?.split("/")[0].trim() || "Mixed Breed",
          breed_secondary: animal.BREED2NAME || undefined,
          breed_mixed: animal.CROSSBREED === 1,
          age_text: animal.AGEGROUP,
          age_months: ageMonths,
          gender: animal.SEXNAME?.toLowerCase().includes("female") ? "female" : "male",
          size,
          weight_lbs: weightLbs,
          color_primary: animal.BASECOLOURNAME,
          description: animal.ANIMALCOMMENTS || animal.WEBSITEMEDIANOTES,
          photo_urls: photoUrls,
          primary_photo_url: photoUrls[0],
          intake_date: intakeDate,
          is_spayed_neutered: animal.NEUTERED === 1,
          good_with_kids: animal.ISGOODWITHCHILDREN === 0 ? undefined : animal.ISGOODWITHCHILDREN === 1,
          good_with_dogs: animal.ISGOODWITHDOGS === 0 ? undefined : animal.ISGOODWITHDOGS === 1,
          good_with_cats: animal.ISGOODWITHCATS === 0 ? undefined : animal.ISGOODWITHCATS === 1,
          house_trained: animal.ISHOUSETRAINED === 0 ? undefined : animal.ISHOUSETRAINED === 1,
          has_special_needs: animal.HASSPECIALNEEDS === 1,
          special_needs_description: animal.HEALTHPROBLEMS,
          external_url: `https://service.sheltermanager.com/asmservice?account=${platformId}&method=animal_view&animalid=${animal.ID}`,
          tags: ["sheltermanager", "asm"],
        });
      }
    } catch {
      // ASM API failed
    }

    return dogs;
  },
};
