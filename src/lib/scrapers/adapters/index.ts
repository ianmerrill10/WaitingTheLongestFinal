/**
 * Adapter Registry — maps platform types to their scraper adapters.
 */
import type { PlatformAdapter, WebsitePlatform } from "../types";
import { shelterluvAdapter } from "./shelterluv";
import { shelterbuddyAdapter } from "./shelterbuddy";
import { petangoAdapter } from "./petango";
import { rescuegroupsWidgetAdapter } from "./rescuegroups-widget";
import { petstablishedAdapter } from "./petstablished";
import { genericHtmlAdapter } from "./generic-html";

const ADAPTERS: Record<string, PlatformAdapter> = {
  shelterluv: shelterluvAdapter,
  shelterbuddy: shelterbuddyAdapter,
  petango: petangoAdapter,
  rescuegroups: rescuegroupsWidgetAdapter,
  petstablished: petstablishedAdapter,
  custom_html: genericHtmlAdapter,
};

/**
 * Get the appropriate adapter for a platform type.
 * Falls back to generic HTML scraper for unknown platforms.
 */
export function getAdapter(platform: WebsitePlatform): PlatformAdapter | null {
  // Skip platforms we can't scrape
  if (
    platform === "facebook_only" ||
    platform === "no_website" ||
    platform === "unreachable" ||
    platform === "adoptapet" // Would need partnership
  ) {
    return null;
  }

  return ADAPTERS[platform] || genericHtmlAdapter;
}

/**
 * Get all available adapters.
 */
export function getAllAdapters(): PlatformAdapter[] {
  return Object.values(ADAPTERS);
}

/**
 * Get adapter names.
 */
export function getAdapterNames(): string[] {
  return Object.keys(ADAPTERS);
}
