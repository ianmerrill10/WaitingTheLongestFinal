/**
 * Platform Detector — visits a shelter URL and identifies what platform they use.
 * Detects: ShelterLuv, ShelterBuddy, Petango, RescueGroups, Petstablished, etc.
 */
import * as cheerio from "cheerio";
import { fetchHTML } from "./rate-limiter";
import type { DetectionResult, WebsitePlatform } from "./types";

interface PlatformSignature {
  platform: WebsitePlatform;
  patterns: RegExp[];
  idExtractor?: (html: string, url: string) => string | null;
}

const PLATFORM_SIGNATURES: PlatformSignature[] = [
  {
    platform: "shelterluv",
    patterns: [
      /shelterluv\.com/i,
      /cdn\.shelterluv\.com/i,
      /www\.shelterluv\.com\/embed/i,
      /data-shelterluv/i,
    ],
    idExtractor: (html: string) => {
      // Look for org ID in shelterluv URLs
      const match =
        html.match(/shelterluv\.com\/(?:available_animals|embed)\/?(\d+)/i) ||
        html.match(/data-org-?id=["']?(\d+)/i) ||
        html.match(/shelter_id["':\s]+["']?(\d+)/i);
      return match ? match[1] : null;
    },
  },
  {
    platform: "shelterbuddy",
    patterns: [
      /shelterbuddy\.com/i,
      /ShelterBuddy/i,
    ],
    idExtractor: (html: string) => {
      const match = html.match(
        /https?:\/\/([^/]+)\.shelterbuddy\.com/i
      );
      return match ? match[1] : null;
    },
  },
  {
    platform: "petango",
    patterns: [
      /petango\.com/i,
      /service\.petango\.com/i,
      /petpoint/i,
    ],
    idExtractor: (html: string) => {
      const match =
        html.match(/petango\.com[^"']*[?&]shelterId=([^"'&]+)/i) ||
        html.match(/petango\.com[^"']*[?&]sId=([^"'&]+)/i);
      return match ? match[1] : null;
    },
  },
  {
    platform: "rescuegroups",
    patterns: [
      /toolkit\.rescuegroups\.org/i,
      /rescuegroups\.org\/toolkit/i,
      /data-rg-/i,
    ],
    idExtractor: (html: string) => {
      const match =
        html.match(/orgID["':\s]+["']?(\d+)/i) ||
        html.match(/rescuegroups\.org[^"']*orgID=(\d+)/i);
      return match ? match[1] : null;
    },
  },
  {
    platform: "petstablished",
    patterns: [
      /petstablished\.com/i,
    ],
    idExtractor: (html: string) => {
      const match = html.match(
        /petstablished\.com\/(?:pets|organization)\/(\d+)/i
      );
      return match ? match[1] : null;
    },
  },
  {
    platform: "adoptapet",
    patterns: [
      /adopt-a-pet\.com/i,
      /adoptapet\.com/i,
    ],
    idExtractor: (html: string) => {
      const match = html.match(
        /adopt(?:-a-)?pet\.com[^"']*[?&](?:shelter_id|did)=([^"'&]+)/i
      );
      return match ? match[1] : null;
    },
  },
];

// Patterns for finding the adoptable animals page
const ADOPTABLE_PAGE_PATTERNS = [
  /href=["']([^"']*(?:adopt(?:able|ion)?|available|our[- ]?dogs|dogs|animals|pets)[^"']*?)["']/gi,
  /href=["']([^"']*(?:meet[- ]?(?:the|our)[- ]?(?:dogs|pets|animals))[^"']*?)["']/gi,
  /href=["']([^"']*(?:find[- ]?a[- ]?(?:dog|pet))[^"']*?)["']/gi,
];

function findAdoptablePage(html: string, baseUrl: string): string | null {
  const candidates: string[] = [];

  for (const pattern of ADOPTABLE_PAGE_PATTERNS) {
    let match;
    // Reset regex state
    pattern.lastIndex = 0;
    while ((match = pattern.exec(html)) !== null) {
      let href = match[1];

      // Skip anchors, emails, tel, javascript
      if (
        href.startsWith("#") ||
        href.startsWith("mailto:") ||
        href.startsWith("tel:") ||
        href.startsWith("javascript:")
      )
        continue;

      // Resolve relative URLs
      try {
        href = new URL(href, baseUrl).toString();
      } catch {
        continue;
      }

      // Skip external links to other domains
      try {
        const hrefDomain = new URL(href).hostname;
        const baseDomain = new URL(baseUrl).hostname;
        if (hrefDomain !== baseDomain) continue;
      } catch {
        continue;
      }

      candidates.push(href);
    }
  }

  // Prefer pages with "dog" in the URL, then "adopt", then "available"
  const scored = candidates.map((url) => {
    let score = 0;
    const lower = url.toLowerCase();
    if (lower.includes("dog")) score += 3;
    if (lower.includes("adopt")) score += 2;
    if (lower.includes("available")) score += 2;
    if (lower.includes("animals") || lower.includes("pets")) score += 1;
    return { url, score };
  });

  scored.sort((a, b) => b.score - a.score);
  return scored.length > 0 ? scored[0].url : null;
}

/**
 * Detect what platform a shelter website uses.
 */
export async function detectPlatform(url: string): Promise<DetectionResult> {
  try {
    const html = await fetchHTML(url);
    const $ = cheerio.load(html);

    // Check for Facebook-only (no real website content)
    if (url.includes("facebook.com") || url.includes("fb.com")) {
      return { platform: "facebook_only", platformId: null, adoptablePageUrl: null };
    }

    // Check each platform signature
    for (const sig of PLATFORM_SIGNATURES) {
      const detected = sig.patterns.some((p) => p.test(html));
      if (detected) {
        const platformId = sig.idExtractor ? sig.idExtractor(html, url) : null;

        // Also check iframes for platform content
        const iframes = $("iframe")
          .map((_, el) => $(el).attr("src") || "")
          .get();
        for (const iframeSrc of iframes) {
          if (sig.patterns.some((p) => p.test(iframeSrc))) {
            const iframeId = sig.idExtractor
              ? sig.idExtractor(iframeSrc, url)
              : null;
            return {
              platform: sig.platform,
              platformId: platformId || iframeId,
              adoptablePageUrl: iframeSrc || url,
            };
          }
        }

        return {
          platform: sig.platform,
          platformId,
          adoptablePageUrl: findAdoptablePage(html, url) || url,
        };
      }
    }

    // No platform detected — custom HTML site
    const adoptablePage = findAdoptablePage(html, url);
    return {
      platform: "custom_html",
      platformId: null,
      adoptablePageUrl: adoptablePage,
    };
  } catch (err) {
    const message = err instanceof Error ? err.message : String(err);
    if (message.includes("429")) {
      return { platform: "unknown", platformId: null, adoptablePageUrl: null };
    }
    return { platform: "unreachable", platformId: null, adoptablePageUrl: null };
  }
}
