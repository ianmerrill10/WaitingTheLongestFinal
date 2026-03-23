/**
 * Chameleon / PetHarbor Adapter
 * Chameleon CMS is used by many municipal shelters. PetHarbor.com is the
 * public-facing search portal. Dogs often include real intake dates.
 */
import * as cheerio from "cheerio";
import { fetchHTML } from "../rate-limiter";
import type { PlatformAdapter, ScrapedDog } from "../types";
import { parseAgeToMonths } from "../dog-mapper";

const PETHARBOR_SEARCH_URL = "https://petharbor.com/results.asp";

export const chameleonAdapter: PlatformAdapter = {
  name: "chameleon",

  detect(html: string, url: string): boolean {
    return /petharbor\.com/i.test(html) || /petharbor\.com/i.test(url) || /chameleonbeach\.com/i.test(html);
  },

  extractPlatformId(html: string, url: string): string | null {
    // PetHarbor shelter IDs are in the URL params
    const match = html.match(/petharbor\.com[^"']*[?&](?:shelter_?id|where)=([A-Z0-9]+)/i)
      || url.match(/[?&](?:shelter_?id|where)=([A-Z0-9]+)/i);
    return match ? match[1] : null;
  },

  async scrape(shelterUrl: string, platformId: string | null): Promise<ScrapedDog[]> {
    const dogs: ScrapedDog[] = [];

    if (!platformId) {
      // Try to extract from the shelter's page
      try {
        const html = await fetchHTML(shelterUrl);
        const match = html.match(/petharbor\.com[^"']*[?&](?:shelter_?id|where)=([A-Z0-9]+)/i);
        if (match) platformId = match[1];
      } catch { /* skip */ }
    }

    if (!platformId) return dogs;

    try {
      // Search PetHarbor for dogs at this shelter
      const searchUrl = `${PETHARBOR_SEARCH_URL}?searchtype=ADOPT&friends=1&saession=&where=${platformId}&PAGE=1&stylesheet=include/default.css&view=sysadm.v_animal_short&bgcolor=ffffff&rows=50&imession=Y&ESSION=&ESSION2=&ESSION3=&nomax=1&fontface=arial&fontsize=10&zip=&miles=&shelession=&aession=&cession=&ESSION2=&ESSION3=&Ession=DOG&color=&sex=&hair=&Ession=&Ession2=&declession=&hidession=`;

      const html = await fetchHTML(searchUrl);
      const $ = cheerio.load(html);

      // PetHarbor results are in table rows
      $("table.ResultsTable tr, table tr").each((idx, el) => {
        try {
          const row = $(el);
          const cells = row.find("td");
          if (cells.length < 3) return;

          // Look for animal link
          const link = row.find("a[href*='pet.asp'], a[href*='detail']").first();
          const href = link.attr("href");
          if (!href) return;

          const idMatch = href.match(/ID=([A-Z0-9-]+)/i);
          if (!idMatch) return;

          const animalId = idMatch[1];
          const name = link.text().trim() || cells.first().text().trim();
          if (!name || name.length < 2) return;

          // Extract breed from cells
          let breed = "Mixed Breed";
          let age = "";
          let sex: "male" | "female" = "male";

          cells.each((_, cell) => {
            const txt = $(cell).text().trim();
            const lower = txt.toLowerCase();
            if (lower.includes("mix") || lower.includes("breed") || /terrier|shepherd|retriever|bulldog|pit/i.test(lower)) {
              breed = txt;
            }
            if (/\d+\s*(year|month|week|yr|mo)/i.test(txt)) {
              age = txt;
            }
            if (lower === "female" || lower === "f" || lower.includes("spayed")) {
              sex = "female";
            }
          });

          // PetHarbor often shows intake date in the ID (format: A123456 or shelter-specific)
          // Or there's an "Impound Date" or "Date In" field
          let intakeDate: string | undefined;
          const dateMatch = row.text().match(/(?:impound|intake|date in|available)[:\s]*(\d{1,2}[/-]\d{1,2}[/-]\d{2,4})/i);
          if (dateMatch) {
            const d = new Date(dateMatch[1]);
            if (!isNaN(d.getTime())) intakeDate = d.toISOString();
          }

          // Photo
          const img = row.find("img").first().attr("src");
          let photoUrl: string | undefined;
          if (img) {
            try { photoUrl = new URL(img, "https://petharbor.com").toString(); } catch { /* skip */ }
          }

          dogs.push({
            external_id: `chameleon-${platformId}-${animalId}`,
            name,
            breed_primary: breed.split("/")[0].trim().substring(0, 100),
            breed_secondary: breed.includes("/") ? breed.split("/")[1].trim() : undefined,
            breed_mixed: breed.toLowerCase().includes("mix"),
            age_text: age || undefined,
            age_months: age ? parseAgeToMonths(age) ?? undefined : undefined,
            gender: sex,
            intake_date: intakeDate,
            photo_urls: photoUrl ? [photoUrl] : [],
            primary_photo_url: photoUrl,
            external_url: href.startsWith("http") ? href : `https://petharbor.com/${href}`,
            tags: ["chameleon", "petharbor"],
          });
        } catch {
          // Skip malformed rows
        }
      });
    } catch {
      // PetHarbor fetch failed
    }

    return dogs;
  },
};
