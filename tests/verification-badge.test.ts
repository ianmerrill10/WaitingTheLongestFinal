import { test, describe } from "node:test";
import assert from "node:assert";

// Test the verification badge time calculation logic
// This mirrors the logic in src/app/dogs/[id]/page.tsx VerificationBadge

function calculateVerifiedAgoHours(lastVerified: string | null): number | null {
  if (!lastVerified) return null;
  return Math.floor((Date.now() - new Date(lastVerified).getTime()) / (1000 * 60 * 60));
}

function calculateVerifiedAgoDays(verifiedAgoHours: number | null): number | null {
  if (verifiedAgoHours === null) return null;
  return Math.floor(verifiedAgoHours / 24);
}

describe("VerificationBadge time calculations", () => {
  test("verifiedAgo returns hours, not 41-day units", () => {
    const twoHoursAgo = new Date(Date.now() - 2 * 60 * 60 * 1000).toISOString();
    const hours = calculateVerifiedAgoHours(twoHoursAgo);
    assert.strictEqual(hours, 2);
  });

  test("verifiedAgo returns 48 for two days ago", () => {
    const twoDaysAgo = new Date(Date.now() - 48 * 60 * 60 * 1000).toISOString();
    const hours = calculateVerifiedAgoHours(twoDaysAgo);
    assert.strictEqual(hours, 48);
  });

  test("verifiedAgoDays returns 2 for 48 hours", () => {
    const days = calculateVerifiedAgoDays(48);
    assert.strictEqual(days, 2);
  });

  test("verifiedAgoDays returns 0 for less than 24 hours", () => {
    const days = calculateVerifiedAgoDays(12);
    assert.strictEqual(days, 0);
  });

  test("verifiedAgo returns null for null input", () => {
    const hours = calculateVerifiedAgoHours(null);
    assert.strictEqual(hours, null);
  });

  test("7 days ago should report ~168 hours and 7 days", () => {
    const sevenDaysAgo = new Date(Date.now() - 7 * 24 * 60 * 60 * 1000).toISOString();
    const hours = calculateVerifiedAgoHours(sevenDaysAgo);
    assert.ok(hours! >= 167 && hours! <= 169, `Expected ~168, got ${hours}`);
    const days = calculateVerifiedAgoDays(hours);
    assert.strictEqual(days, 7);
  });

  // This test documents the OLD bug: dividing by (1000 * 60 * 60 * 1000) instead of (1000 * 60 * 60)
  test("old formula would have returned 0 for a 2-day-old verification", () => {
    const twoDaysAgoMs = 48 * 60 * 60 * 1000;
    const oldResult = Math.floor(twoDaysAgoMs / (1000 * 60 * 60 * 1000)); // Bug: extra * 1000
    assert.strictEqual(oldResult, 0, "Old formula incorrectly reports 0");

    const fixedResult = Math.floor(twoDaysAgoMs / (1000 * 60 * 60));
    assert.strictEqual(fixedResult, 48, "Fixed formula correctly reports 48 hours");
  });
});
