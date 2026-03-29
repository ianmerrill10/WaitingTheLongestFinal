import test from "node:test";
import assert from "node:assert/strict";
import { parseDateFromDescription } from "@/lib/utils/parse-description-date";

test("parseDateFromDescription extracts posted date with high confidence", () => {
  const result = parseDateFromDescription("Sweet dog. Posted 2/18/18 and doing well in foster.");

  assert.ok(result);
  assert.equal(result?.confidence, "high");
  assert.equal(result?.date.toISOString().slice(0, 10), "2018-02-18");
});

test("parseDateFromDescription extracts month-year rescue date", () => {
  const result = parseDateFromDescription("Bella has been in rescue since March 2019 and deserves a home.");

  assert.ok(result);
  assert.equal(result?.date.toISOString().slice(0, 10), "2019-03-01");
});

test("parseDateFromDescription ignores future dates", () => {
  const nextYear = new Date().getFullYear() + 1;
  const result = parseDateFromDescription(`Available since January 5, ${nextYear}`);

  if (nextYear > new Date().getFullYear()) {
    assert.equal(result, null);
  }
});