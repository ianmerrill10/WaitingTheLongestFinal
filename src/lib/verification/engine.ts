/**
 * Dog Verification Engine
 *
 * Verifies that dogs listed on our site still actually exist and are
 * available at their shelters. Uses two strategies:
 *
 * 1. RescueGroups API re-check — query the animal by external_id
 *    - If found: mark verified, capture birthDate, update last_verified_at
 *    - If not found (404): dog was adopted/removed → mark not_found
 *    - If API error: skip, try again next run
 *
 * 2. External URL liveness — HEAD request to the dog's listing page
 *    - If 404/gone: corroborating evidence the dog was removed
 *    - If 200: dog's page still exists (but doesn't prove availability)
 *
 * Dogs confirmed not_found by BOTH checks get auto-deactivated.
 * Dogs only failing one check get flagged for manual review.
 *
 * Prioritization: oldest dogs first, never-verified dogs first.
 * Throughput: parallel batches of 5, ~400 dogs/day across both crons.
 */

import { createAdminClient } from "@/lib/supabase/admin";
import { createRescueGroupsClient, type RGAnimalAttributes } from "@/lib/rescuegroups/client";

export interface VerificationResult {
  checked: number;
  verified: number;
  notFound: number;
  deactivated: number;
  pending: number;
  errors: number;
  birthDatesCaptured: number;
  duration: number;
}

interface DogToVerify {
  id: string;
  name: string;
  external_id: string;
  external_url: string | null;
  intake_date: string;
  verification_status: string | null;
  last_verified_at: string | null;
  age_months: number | null;
  birth_date: string | null;
}

type VerifyOutcome = "verified" | "not_found" | "deactivated" | "pending" | "error";

/**
 * Run verification on a batch of dogs.
 * @param batchSize How many dogs to verify in this run
 * @param prioritize Which dogs to check first: 'oldest' | 'never_verified' | 'stale'
 */
export async function runVerification(
  batchSize: number = 50,
  prioritize: "oldest" | "never_verified" | "stale" = "never_verified"
): Promise<VerificationResult> {
  const startTime = Date.now();
  const supabase = createAdminClient();
  const rgClient = createRescueGroupsClient();

  let checked = 0;
  let verified = 0;
  let notFound = 0;
  let deactivated = 0;
  let pending = 0;
  let errors = 0;
  let birthDatesCaptured = 0;

  // Fetch dogs to verify — prioritize based on strategy
  let query = supabase
    .from("dogs")
    .select("id, name, external_id, external_url, intake_date, verification_status, last_verified_at, age_months, birth_date")
    .eq("is_available", true)
    .eq("external_source", "rescuegroups")
    .not("external_id", "is", null)
    .limit(batchSize);

  switch (prioritize) {
    case "oldest":
      query = query.order("intake_date", { ascending: true });
      break;
    case "never_verified":
      query = query
        .is("last_verified_at", null)
        .order("intake_date", { ascending: true });
      break;
    case "stale": {
      const sevenDaysAgo = new Date(Date.now() - 7 * 24 * 60 * 60 * 1000).toISOString();
      query = query
        .or(`last_verified_at.is.null,last_verified_at.lt.${sevenDaysAgo}`)
        .order("last_verified_at", { ascending: true, nullsFirst: true });
      break;
    }
  }

  const { data: dogs, error: fetchError } = await query;

  if (fetchError || !dogs || dogs.length === 0) {
    return { checked: 0, verified: 0, notFound: 0, deactivated: 0, pending: 0, errors: 0, birthDatesCaptured: 0, duration: Date.now() - startTime };
  }

  const now = new Date().toISOString();

  // Process in parallel batches of 5 for throughput
  const PARALLEL_BATCH = 5;
  for (let i = 0; i < dogs.length; i += PARALLEL_BATCH) {
    const batch = dogs.slice(i, i + PARALLEL_BATCH);
    const results = await Promise.allSettled(
      batch.map(dog => verifyOneDog(dog as DogToVerify, rgClient, supabase, now))
    );
    for (const r of results) {
      checked++;
      if (r.status === "fulfilled") {
        const { outcome, capturedBirthDate } = r.value;
        if (outcome === "verified") verified++;
        else if (outcome === "deactivated") { deactivated++; notFound++; }
        else if (outcome === "not_found") notFound++;
        else if (outcome === "pending") pending++;
        else if (outcome === "error") errors++;
        if (capturedBirthDate) birthDatesCaptured++;
      } else {
        errors++;
      }
    }
  }

  return {
    checked,
    verified,
    notFound,
    deactivated,
    pending,
    errors,
    birthDatesCaptured,
    duration: Date.now() - startTime,
  };
}

/**
 * Verify a single dog against RescueGroups API.
 * Also opportunistically captures birthDate and isCourtesyListing.
 */
async function verifyOneDog(
  dog: DogToVerify,
  rgClient: ReturnType<typeof createRescueGroupsClient>,
  supabase: ReturnType<typeof createAdminClient>,
  now: string,
): Promise<{ outcome: VerifyOutcome; capturedBirthDate: boolean }> {
  const rgResult = await rgClient.verifyAnimal(dog.external_id);

  if (rgResult.status === "available") {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const updateData: Record<string, any> = {
      verification_status: "verified",
      last_verified_at: now,
    };

    let capturedBirthDate = false;

    if (rgResult.animal) {
      const attrs = rgResult.animal.attributes as RGAnimalAttributes;

      // Opportunistically capture birthDate
      if (attrs.birthDate && !dog.birth_date) {
        const bd = new Date(attrs.birthDate);
        if (!isNaN(bd.getTime())) {
          updateData.birth_date = attrs.birthDate;
          updateData.is_birth_date_exact = attrs.isBirthDateExact ?? null;
          const nowDate = new Date();
          const ageMonths = Math.max(0,
            (nowDate.getFullYear() - bd.getFullYear()) * 12 +
            (nowDate.getMonth() - bd.getMonth()));
          if (dog.age_months === null || attrs.isBirthDateExact) {
            updateData.age_months = ageMonths;
          }
          capturedBirthDate = true;

          // Age sanity: if intake_date is before birth, cap it
          const intakeDate = new Date(dog.intake_date);
          if (intakeDate < bd) {
            updateData.intake_date = bd.toISOString();
            updateData.date_confidence = "low";
            updateData.date_source = `age_capped_api_birth_date${attrs.isBirthDateExact ? "_exact" : ""}`;
          }
        }
      }

      // Capture courtesy listing flag
      if (attrs.isCourtesyListing !== undefined) {
        updateData.is_courtesy_listing = attrs.isCourtesyListing;
      }
    }

    await supabase.from("dogs").update(updateData).eq("id", dog.id);
    return { outcome: "verified", capturedBirthDate };
  }

  if (rgResult.status === "pending") {
    await supabase
      .from("dogs")
      .update({
        verification_status: "pending",
        last_verified_at: now,
        status: "pending",
      })
      .eq("id", dog.id);
    return { outcome: "pending", capturedBirthDate: false };
  }

  if (rgResult.status === "api_error") {
    return { outcome: "error", capturedBirthDate: false };
  }

  // Status is "not_found" — dog no longer in RescueGroups
  let urlAlive = false;
  if (dog.external_url) {
    urlAlive = await checkUrlAlive(dog.external_url);
  }

  if (!urlAlive) {
    await supabase
      .from("dogs")
      .update({
        verification_status: "not_found",
        last_verified_at: now,
        is_available: false,
        status: "adopted",
      })
      .eq("id", dog.id);
    return { outcome: "deactivated", capturedBirthDate: false };
  } else {
    await supabase
      .from("dogs")
      .update({
        verification_status: "not_found",
        last_verified_at: now,
      })
      .eq("id", dog.id);
    return { outcome: "not_found", capturedBirthDate: false };
  }
}

/**
 * Check if a URL is still live (returns 200-399).
 */
async function checkUrlAlive(url: string): Promise<boolean> {
  try {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 5000);

    const response = await fetch(url, {
      method: "HEAD",
      signal: controller.signal,
      redirect: "follow",
      headers: {
        "User-Agent": "WaitingTheLongest.com/1.0 (dog-adoption-verification)",
      },
    });

    clearTimeout(timeout);
    return response.status >= 200 && response.status < 400;
  } catch {
    return false;
  }
}

/**
 * Get verification statistics for the dashboard.
 */
export async function getVerificationStats(): Promise<{
  total: number;
  verified: number;
  unverified: number;
  notFound: number;
  pending: number;
  neverChecked: number;
  stale: number;
}> {
  const supabase = createAdminClient();

  const [totalRes, verifiedRes, notFoundRes, pendingRes, neverCheckedRes, staleRes] =
    await Promise.all([
      supabase
        .from("dogs")
        .select("id", { count: "exact", head: true })
        .eq("is_available", true),
      supabase
        .from("dogs")
        .select("id", { count: "exact", head: true })
        .eq("is_available", true)
        .eq("verification_status", "verified"),
      supabase
        .from("dogs")
        .select("id", { count: "exact", head: true })
        .eq("is_available", true)
        .eq("verification_status", "not_found"),
      supabase
        .from("dogs")
        .select("id", { count: "exact", head: true })
        .eq("is_available", true)
        .eq("verification_status", "pending"),
      supabase
        .from("dogs")
        .select("id", { count: "exact", head: true })
        .eq("is_available", true)
        .is("last_verified_at", null),
      supabase
        .from("dogs")
        .select("id", { count: "exact", head: true })
        .eq("is_available", true)
        .lt("last_verified_at", new Date(Date.now() - 7 * 24 * 60 * 60 * 1000).toISOString()),
    ]);

  const total = totalRes.count ?? 0;
  return {
    total,
    verified: verifiedRes.count ?? 0,
    unverified: total - (verifiedRes.count ?? 0) - (notFoundRes.count ?? 0) - (pendingRes.count ?? 0),
    notFound: notFoundRes.count ?? 0,
    pending: pendingRes.count ?? 0,
    neverChecked: neverCheckedRes.count ?? 0,
    stale: staleRes.count ?? 0,
  };
}
