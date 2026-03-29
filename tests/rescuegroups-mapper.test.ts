import test from "node:test";
import assert from "node:assert/strict";
import { mapRescueGroupsDog } from "@/lib/rescuegroups/mapper";
import type { RGAnimalAttributes } from "@/lib/rescuegroups/client";

function buildAttrs(overrides: Partial<RGAnimalAttributes> = {}): RGAnimalAttributes {
  return {
    name: "Milo",
    breedPrimary: "Labrador Retriever",
    ageGroup: "Adult",
    sex: "Male",
    sizeGroup: "Large",
    descriptionText: "Friendly dog",
    pictureThumbnailUrl: null,
    createdDate: "2024-03-01T00:00:00.000Z",
    url: "https://example.org/dogs/milo",
    ...overrides,
  };
}

test("mapRescueGroupsDog prefers availableDate as verified intake date", () => {
  const dog = mapRescueGroupsDog(
    "rg-1",
    buildAttrs({
      availableDate: "2024-02-10T00:00:00.000Z",
      createdDate: "2024-03-01T00:00:00.000Z",
    })
  );

  assert.equal(dog.date_confidence, "verified");
  assert.equal(dog.date_source, "rescuegroups_available_date");
  assert.equal(dog.intake_date.slice(0, 10), "2024-02-10");
});

test("mapRescueGroupsDog falls back to foundDate when availableDate is missing", () => {
  const dog = mapRescueGroupsDog(
    "rg-2",
    buildAttrs({
      foundDate: "2024-01-15T00:00:00.000Z",
      createdDate: "2024-03-01T00:00:00.000Z",
    })
  );

  assert.equal(dog.date_confidence, "verified");
  assert.equal(dog.date_source, "rescuegroups_found_date");
  assert.equal(dog.intake_type, "stray");
});

test("mapRescueGroupsDog detects returned-after-adoption cases", () => {
  const dog = mapRescueGroupsDog(
    "rg-3",
    buildAttrs({
      adoptedDate: "2024-01-01T00:00:00.000Z",
      updatedDate: "2024-02-15T00:00:00.000Z",
      createdDate: "2023-12-01T00:00:00.000Z",
    })
  );

  assert.equal(dog.date_confidence, "high");
  assert.equal(dog.date_source, "rescuegroups_returned_after_adoption");
  assert.equal(dog.intake_date.slice(0, 10), "2024-02-15");
});

test("mapRescueGroupsDog caps impossible intake dates by age", () => {
  const dog = mapRescueGroupsDog(
    "rg-4",
    buildAttrs({
      birthDate: "2024-01-01T00:00:00.000Z",
      isBirthDateExact: true,
      availableDate: "2023-01-01T00:00:00.000Z",
      createdDate: "2024-03-01T00:00:00.000Z",
    })
  );

  assert.equal(dog.date_confidence, "low");
  assert.match(dog.date_source, /age_capped_api/);
  assert.match(dog.date_source, /wait_capped_365d/);

  const waitDays = (Date.now() - new Date(dog.intake_date).getTime()) / 86400000;
  assert.ok(waitDays >= 364 && waitDays <= 366, `expected capped wait near 365 days, got ${waitDays}`);
});