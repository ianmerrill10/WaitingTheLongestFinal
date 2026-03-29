/**
 * State-specific stray hold periods (in days).
 *
 * When a stray dog enters a municipal shelter, the shelter must hold the dog
 * for a legally mandated period before making it available for adoption.
 * Many shelters report "available_date" (after hold) rather than true intake.
 *
 * These values represent the most common statutory hold period per state.
 * Individual municipalities may have longer holds — per-shelter overrides
 * are stored in shelters.stray_hold_days.
 *
 * Sources: State animal control statutes, ASPCA research, 2024-2025 data.
 */
export const STATE_STRAY_HOLD_DAYS: Record<string, number> = {
  AL: 7, AK: 3, AZ: 3, AR: 5, CA: 5,
  CO: 5, CT: 7, DE: 5, FL: 5, GA: 5,
  HI: 2, ID: 5, IL: 7, IN: 3, IA: 7,
  KS: 3, KY: 5, LA: 5, ME: 6, MD: 7,
  MA: 10, MI: 4, MN: 5, MS: 5, MO: 5,
  MT: 3, NE: 5, NV: 3, NH: 7, NJ: 7,
  NM: 3, NY: 5, NC: 3, ND: 3, OH: 3,
  OK: 5, OR: 5, PA: 2, RI: 5, SC: 5,
  SD: 5, TN: 3, TX: 3, UT: 5, VT: 10,
  VA: 5, WA: 3, WV: 5, WI: 7, WY: 5,
};

/** Default stray hold if state not found */
export const DEFAULT_STRAY_HOLD_DAYS = 5;

/**
 * Get stray hold days for a shelter.
 * Per-shelter override takes priority, then state default.
 */
export function getStrayHoldDays(
  stateCode: string,
  shelterOverride?: number | null
): number {
  if (shelterOverride != null && shelterOverride > 0) return shelterOverride;
  return STATE_STRAY_HOLD_DAYS[stateCode.toUpperCase()] ?? DEFAULT_STRAY_HOLD_DAYS;
}
