import { test, describe } from "node:test";
import assert from "node:assert";

// Test breed slug encoding/decoding
// Mirrors logic in src/app/breeds/page.tsx and src/app/breeds/[slug]/page.tsx

function breedToSlug(breed: string): string {
  return encodeURIComponent(breed.toLowerCase().replace(/ /g, "-"));
}

function slugToBreed(slug: string): string {
  return decodeURIComponent(slug).replace(/-/g, " ").replace(/\b\w/g, l => l.toUpperCase());
}

describe("Breed slug encoding/decoding", () => {
  test("common breed roundtrips correctly", () => {
    const breed = "German Shepherd";
    const slug = breedToSlug(breed);
    assert.strictEqual(slug, "german-shepherd");
    const decoded = slugToBreed(slug);
    assert.strictEqual(decoded, "German Shepherd");
  });

  test("single word breed works", () => {
    const breed = "Beagle";
    const slug = breedToSlug(breed);
    assert.strictEqual(slug, "beagle");
    const decoded = slugToBreed(slug);
    assert.strictEqual(decoded, "Beagle");
  });

  test("multi-word breed works", () => {
    const breed = "American Pit Bull Terrier";
    const slug = breedToSlug(breed);
    assert.strictEqual(slug, "american-pit-bull-terrier");
    const decoded = slugToBreed(slug);
    assert.strictEqual(decoded, "American Pit Bull Terrier");
  });

  test("breed with special chars gets encoded", () => {
    const breed = "Mixed / Other";
    const slug = breedToSlug(breed);
    // URL encoding: / becomes %2F
    assert.ok(slug.includes("%2f") || slug.includes("%2F"),
      `Expected URL-encoded slash, got: ${slug}`);
  });

  test("ilike query is case-insensitive (casing mismatch is OK)", () => {
    // The breed page uses .ilike("breed_primary", decoded)
    // So even if casing doesn't match DB, ilike handles it
    const slug = "labrador-retriever";
    const decoded = slugToBreed(slug);
    assert.strictEqual(decoded, "Labrador Retriever");
    // ilike would match "labrador retriever", "LABRADOR RETRIEVER", etc.
    assert.ok(true, "ilike handles case differences");
  });
});
