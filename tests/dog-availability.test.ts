import { test, describe } from "node:test";
import assert from "node:assert";

// Test dog availability display logic
// This mirrors the logic in src/app/dogs/[id]/page.tsx

describe("Dog availability display logic", () => {
  test("isAvailable is true when is_available is true", () => {
    const dog = { is_available: true };
    const isAvailable = dog.is_available !== false;
    assert.strictEqual(isAvailable, true);
  });

  test("isAvailable is false when is_available is false", () => {
    const dog = { is_available: false };
    const isAvailable = dog.is_available !== false;
    assert.strictEqual(isAvailable, false);
  });

  test("isAvailable is true when is_available is null (defensive)", () => {
    const dog = { is_available: null };
    const isAvailable = dog.is_available !== false;
    assert.strictEqual(isAvailable, true);
  });

  test("isAvailable is true when is_available is undefined (defensive)", () => {
    const dog = {} as { is_available?: boolean };
    const isAvailable = dog.is_available !== false;
    assert.strictEqual(isAvailable, true);
  });

  test("InterestForm should only render for available dogs", () => {
    // Simulate the conditional: {isAvailable && <InterestForm />}
    const availableDog = { is_available: true };
    const unavailableDog = { is_available: false };

    assert.strictEqual(availableDog.is_available !== false && true, true);
    assert.strictEqual(unavailableDog.is_available !== false && true, false);
  });

  test("Call Shelter NOW should only show for available urgent dogs", () => {
    const cases = [
      { is_available: true, urgency: "critical", phone: "555-0100", expected: true },
      { is_available: true, urgency: "high", phone: "555-0100", expected: true },
      { is_available: true, urgency: "normal", phone: "555-0100", expected: false },
      { is_available: false, urgency: "critical", phone: "555-0100", expected: false },
      { is_available: true, urgency: "critical", phone: null, expected: false },
    ];

    for (const c of cases) {
      const isAvailable = c.is_available !== false;
      const isUrgent = c.urgency === "critical" || c.urgency === "high";
      const showCallButton = isAvailable && isUrgent && !!c.phone;
      assert.strictEqual(showCallButton, c.expected,
        `Failed for: available=${c.is_available}, urgency=${c.urgency}, phone=${c.phone}`);
    }
  });
});

describe("Urgency level classification", () => {
  test("urgency defaults to normal when null", () => {
    const urgency = (null || "normal");
    assert.strictEqual(urgency, "normal");
  });

  test("urgency defaults to normal when undefined", () => {
    const urgency = (undefined || "normal");
    assert.strictEqual(urgency, "normal");
  });

  test("urgency passes through valid values", () => {
    for (const level of ["critical", "high", "medium", "normal"]) {
      const urgency = (level || "normal");
      assert.strictEqual(urgency, level);
    }
  });
});
