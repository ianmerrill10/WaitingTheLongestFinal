/**
 * Generic HTML Scraper — the fallback adapter for custom shelter websites.
 * Uses heuristics to find dog listings on unknown HTML structures.
 * This handles ~96% of shelter websites that use custom domains.
 */
import * as cheerio from "cheerio";
import { fetchHTML } from "../rate-limiter";
import type { PlatformAdapter, ScrapedDog } from "../types";

// Common words that indicate a dog listing
const DOG_NAME_PATTERN = /^[A-Z][a-z]{1,20}(?:\s[A-Z][a-z]{1,20})?$/;
const BREED_KEYWORDS = [
  "breed", "mix", "terrier", "shepherd", "retriever", "bulldog",
  "poodle", "beagle", "husky", "lab", "labrador", "pitbull", "pit bull",
  "chihuahua", "dachshund", "boxer", "collie", "spaniel", "hound",
  "corgi", "rottweiler", "doberman", "mastiff", "dane", "pug",
  "shih tzu", "yorkie", "yorkshire", "maltese", "schnauzer",
];

const ADOPTABLE_LINK_KEYWORDS = [
  "adopt", "available", "dogs", "animals", "pets", "our-dogs",
  "our_dogs", "meet", "find", "adoptable", "rescue",
];

/**
 * Find the "adoptable animals" page from a shelter's homepage.
 */
async function findAdoptablePage(
  baseUrl: string,
  html: string
): Promise<{ url: string; html: string } | null> {
  const $ = cheerio.load(html);
  const links: { href: string; score: number }[] = [];

  $("a[href]").each((_, el) => {
    const href = $(el).attr("href");
    const text = $(el).text().toLowerCase().trim();
    if (!href) return;

    // Skip non-page links
    if (
      href.startsWith("#") ||
      href.startsWith("mailto:") ||
      href.startsWith("tel:") ||
      href.startsWith("javascript:")
    )
      return;

    let score = 0;
    const hrefLower = href.toLowerCase();
    const combined = hrefLower + " " + text;

    for (const kw of ADOPTABLE_LINK_KEYWORDS) {
      if (combined.includes(kw)) score += 2;
    }
    if (text.includes("dog")) score += 3;
    if (combined.includes("available")) score += 2;

    if (score > 0) {
      try {
        const fullUrl = new URL(href, baseUrl).toString();
        // Only follow same-domain links
        if (new URL(fullUrl).hostname === new URL(baseUrl).hostname) {
          links.push({ href: fullUrl, score });
        }
      } catch {
        // Invalid URL
      }
    }
  });

  links.sort((a, b) => b.score - a.score);

  // Try the top 3 candidates
  for (const link of links.slice(0, 3)) {
    try {
      const pageHtml = await fetchHTML(link.href);
      // Quick check: does this page have dog-like content?
      const lower = pageHtml.toLowerCase();
      const dogSignals =
        (lower.match(/adopt/g) || []).length +
        (lower.match(/breed/g) || []).length +
        (lower.match(/spay|neuter/g) || []).length;

      if (dogSignals >= 2) {
        return { url: link.href, html: pageHtml };
      }
    } catch {
      continue;
    }
  }

  return null;
}

/**
 * Extract dogs from an HTML page using heuristics.
 */
function extractDogsFromHTML(
  html: string,
  pageUrl: string,
  shelterId: string
): ScrapedDog[] {
  const $ = cheerio.load(html);
  const dogs: ScrapedDog[] = [];
  const hostname = new URL(pageUrl).hostname.replace(/\./g, "-");

  // Strategy 1: Look for repeated card/list-item patterns with images
  const cardSelectors = [
    ".pet-card", ".animal-card", ".dog-card",
    ".pet-listing", ".animal-listing",
    ".grid-item", ".card", ".pet",
    "[class*='pet-card']", "[class*='animal']",
    "article", ".post", ".entry",
  ];

  for (const selector of cardSelectors) {
    const cards = $(selector);
    if (cards.length < 2) continue; // Need at least 2 to be a list

    cards.each((idx, el) => {
      try {
        const card = $(el);
        const text = card.text();

        // Must have an image (dogs have photos)
        const img =
          card.find("img").first().attr("src") ||
          card.find("img").first().attr("data-src");

        // Extract name (usually in a heading)
        let name =
          card.find("h1, h2, h3, h4, h5").first().text().trim() ||
          card.find("a").first().text().trim() ||
          card.find(".name, .title").first().text().trim();

        // Clean up name
        name = name.replace(/\s+/g, " ").trim();
        if (!name || name.length < 2 || name.length > 50) return;
        // Skip non-dog content
        if (/^(home|about|contact|donate|volunteer|news|blog|events)/i.test(name)) return;

        // Look for breed info
        let breed = "Mixed Breed";
        const breedEl = card.find(
          ".breed, [class*='breed'], .pet-breed"
        );
        if (breedEl.length) {
          breed = breedEl.text().trim();
        } else {
          // Check if any text contains breed keywords
          const textLower = text.toLowerCase();
          for (const bk of BREED_KEYWORDS) {
            if (textLower.includes(bk)) {
              breed = bk.charAt(0).toUpperCase() + bk.slice(1);
              break;
            }
          }
        }

        // Look for age
        const ageEl = card.find(".age, [class*='age'], .pet-age");
        const ageText = ageEl.length ? ageEl.text().trim() : undefined;

        // Look for gender
        const genderEl = card.find(
          ".sex, .gender, [class*='sex'], [class*='gender']"
        );
        let gender: "male" | "female" = "male";
        if (genderEl.length) {
          gender = genderEl.text().toLowerCase().includes("female")
            ? "female"
            : "male";
        } else if (text.toLowerCase().includes("female") || text.toLowerCase().includes("girl")) {
          gender = "female";
        }

        // Get detail link
        const link = card.find("a").first().attr("href");
        let detailUrl: string | undefined;
        if (link) {
          try {
            detailUrl = new URL(link, pageUrl).toString();
          } catch {
            // Invalid URL
          }
        }

        // Build photo URL
        let photoUrl: string | undefined;
        if (img) {
          try {
            photoUrl = new URL(img, pageUrl).toString();
          } catch {
            photoUrl = img;
          }
        }

        // Look for size
        const sizeEl = card.find(".size, [class*='size'], .pet-size");
        let size: ScrapedDog["size"] | undefined;
        if (sizeEl.length) {
          const sizeText = sizeEl.text().toLowerCase().trim();
          if (sizeText.includes("xlarge") || sizeText.includes("extra large") || sizeText.includes("x-large") || sizeText.includes("giant")) size = "xlarge";
          else if (sizeText.includes("small")) size = "small";
          else if (sizeText.includes("large")) size = "large";
          else if (sizeText.includes("medium")) size = "medium";
        }

        // Look for description text
        const descEl = card.find(".description, .bio, [class*='desc'], [class*='bio'], p");
        let description: string | undefined;
        if (descEl.length) {
          const descText = descEl.first().text().trim();
          if (descText.length > 20 && descText.length < 5000) description = descText;
        }

        // Extract behavioral keywords from description text
        let good_with_kids: boolean | undefined;
        let good_with_dogs: boolean | undefined;
        let good_with_cats: boolean | undefined;
        let house_trained: boolean | undefined;
        const combinedText = (description || text).toLowerCase();
        if (/good with (?:children|kids)|kid[- ]?friendly/i.test(combinedText)) good_with_kids = true;
        if (/good with (?:other )?dogs|dog[- ]?friendly/i.test(combinedText)) good_with_dogs = true;
        if (/good with cats|cat[- ]?friendly/i.test(combinedText)) good_with_cats = true;
        if (/house ?trained|potty ?trained|housebroken/i.test(combinedText)) house_trained = true;

        // Collect multiple photos
        const photoUrls: string[] = [];
        card.find("img").each((_, imgEl) => {
          const src = $(imgEl).attr("src") || $(imgEl).attr("data-src");
          if (src && !src.includes("placeholder") && !src.includes("no-photo") && !src.includes("logo")) {
            try {
              photoUrls.push(new URL(src, pageUrl).toString());
            } catch { /* skip */ }
          }
        });

        const id = `generic-${hostname}-${idx}-${name.replace(/\s+/g, "-").toLowerCase().substring(0, 30)}`;

        dogs.push({
          external_id: id,
          name,
          breed_primary: breed.split("/")[0].trim().substring(0, 100),
          breed_mixed: breed.toLowerCase().includes("mix"),
          age_text: ageText,
          gender,
          size,
          description,
          photo_urls: photoUrls.length > 0 ? photoUrls : (photoUrl ? [photoUrl] : []),
          primary_photo_url: photoUrls[0] || photoUrl,
          external_url: detailUrl,
          good_with_kids,
          good_with_dogs,
          good_with_cats,
          house_trained,
          tags: ["generic-scraper"],
        });
      } catch {
        // Skip malformed card
      }
    });

    // If we found dogs with this selector, don't try others
    if (dogs.length >= 2) break;
  }

  // Strategy 2: If no cards found, look for a structured list/table
  if (dogs.length === 0) {
    $("table tbody tr").each((idx, el) => {
      try {
        const row = $(el);
        const cells = row.find("td");
        if (cells.length < 2) return;

        const firstCell = cells.first().text().trim();
        if (!firstCell || firstCell.length < 2 || firstCell.length > 50) return;
        if (/^(name|dog|pet|animal|#|\d+$)/i.test(firstCell)) return; // Skip header

        // Check if this looks like a dog name
        if (!DOG_NAME_PATTERN.test(firstCell) && !/[a-zA-Z]/.test(firstCell)) return;

        const breed = cells.eq(1).text().trim() || "Mixed Breed";
        const ageText = cells.eq(2).text().trim() || undefined;
        const img = row.find("img").first().attr("src");
        const link = row.find("a").first().attr("href");

        const id = `generic-${hostname}-tbl-${idx}-${firstCell.replace(/\s+/g, "-").toLowerCase().substring(0, 30)}`;

        dogs.push({
          external_id: id,
          name: firstCell,
          breed_primary: breed.split("/")[0].trim().substring(0, 100),
          age_text: ageText,
          photo_urls: img ? [new URL(img, pageUrl).toString()] : [],
          external_url: link
            ? new URL(link, pageUrl).toString()
            : undefined,
          tags: ["generic-scraper", "table"],
        });
      } catch {
        // Skip
      }
    });
  }

  return dogs;
}

export const genericHtmlAdapter: PlatformAdapter = {
  name: "generic_html",

  detect(): boolean {
    return true; // Fallback — always matches
  },

  extractPlatformId(): string | null {
    return null;
  },

  async scrape(shelterUrl: string): Promise<ScrapedDog[]> {
    try {
      const html = await fetchHTML(shelterUrl);

      // First try to extract from the main page
      let dogs = extractDogsFromHTML(html, shelterUrl, "generic");

      // If no dogs found, try to find the adoptable animals page
      if (dogs.length < 2) {
        const adoptablePage = await findAdoptablePage(shelterUrl, html);
        if (adoptablePage) {
          const pageDogs = extractDogsFromHTML(
            adoptablePage.html,
            adoptablePage.url,
            "generic"
          );
          if (pageDogs.length > dogs.length) {
            dogs = pageDogs;
          }
        }
      }

      // Also check for iframes that might contain embedded platforms
      const $ = cheerio.load(html);
      const iframes = $("iframe[src]")
        .map((_, el) => $(el).attr("src"))
        .get()
        .filter(Boolean);

      for (const iframeSrc of iframes.slice(0, 3)) {
        try {
          const iframeHtml = await fetchHTML(iframeSrc);
          const iframeDogs = extractDogsFromHTML(
            iframeHtml,
            iframeSrc,
            "generic-iframe"
          );
          if (iframeDogs.length > dogs.length) {
            dogs = iframeDogs;
          }
        } catch {
          // iframe fetch failed
        }
      }

      return dogs;
    } catch {
      return [];
    }
  },
};
