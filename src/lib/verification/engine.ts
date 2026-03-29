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
import { MAX_WAIT_DAYS, type DateConfidence } from "@/lib/rescuegroups/mapper";

export interface VerificationResult {
  checked: number;
  verified: number;
  notFound: number;
  deactivated: number;
  pending: number;
  errors: number;
  birthDatesCaptured: number;
  deadExternalUrls: number;
  availableDatesCaptured: number;
  foundDatesCaptured: number;
  killDatesCaptured: number;
  datesCapped: number;
  duration: number;
}

interface SourceLink {
  url: string;
  source: string;
  checked_at: string;
  status_code: number | null;
  description: string;
}

interface DogToVerify {
  id: string;
  name: string;
  external_id: string;
  external_url: string | null;
  intake_date: string;
  original_intake_date?: string | null;
  intake_date_observation_count?: number | null;
  date_confidence: DateConfidence | null;
  date_source: string | null;
  verification_status: string | null;
  last_verified_at: string | null;
  age_months: number | null;
  birth_date: string | null;
  breed_primary?: string | null;
  primary_photo_url?: string | null;
  shelter_id?: string | null;
  source_links?: SourceLink[] | null;
  page_hash?: string | null;
}

/**
 * Compute a simple hash of page content for change detection.
 * Uses Web Crypto API (available in Node 18+).
 */
async function computePageHash(content: string): Promise<string> {
  const encoder = new TextEncoder();
  const data = encoder.encode(content);
  const hashBuffer = await crypto.subtle.digest("SHA-256", data);
  const hashArray = Array.from(new Uint8Array(hashBuffer));
  return hashArray.map((b) => b.toString(16).padStart(2, "0")).join("");
}

export type VerifyOutcome = "verified" | "not_found" | "deactivated" | "pending" | "error";

export interface DogVerificationEvent {
  dog: {
    id: string;
    name: string;
    breed_primary: string | null;
    primary_photo_url: string | null;
    shelter_id: string | null;
    external_id: string;
    intake_date: string;
  };
  outcome: VerifyOutcome;
  capturedBirthDate: boolean;
  capturedAvailableDate: boolean;
  capturedFoundDate: boolean;
  capturedKillDate: boolean;
  externalUrlDead: boolean;
  error?: string;
}

/**
 * Run verification on a batch of dogs.
 * @param batchSize How many dogs to verify in this run
 * @param prioritize Which dogs to check first: 'oldest' | 'never_verified' | 'stale'
 */
export async function runVerification(
  batchSize: number = 50,
  prioritize: "oldest" | "never_verified" | "stale" = "never_verified",
  onDogProcessed?: (event: DogVerificationEvent) => void,
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
  let deadExternalUrls = 0;
  let availableDatesCaptured = 0;
  let foundDatesCaptured = 0;
  let killDatesCaptured = 0;
  let datesCapped = 0;

  // Fetch dogs to verify — prioritize based on strategy
  let query = supabase
    .from("dogs")
    .select("id, name, external_id, external_url, intake_date, original_intake_date, intake_date_observation_count, date_confidence, date_source, verification_status, last_verified_at, age_months, birth_date, breed_primary, primary_photo_url, shelter_id, source_links, page_hash")
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
    return { checked: 0, verified: 0, notFound: 0, deactivated: 0, pending: 0, errors: 0, birthDatesCaptured: 0, deadExternalUrls: 0, availableDatesCaptured: 0, foundDatesCaptured: 0, killDatesCaptured: 0, datesCapped: 0, duration: Date.now() - startTime };
  }

  const now = new Date().toISOString();

  // Process in parallel batches of 15 for high throughput
  // (32GB RAM + i7 can handle much more concurrency)
  const PARALLEL_BATCH = 15;
  for (let i = 0; i < dogs.length; i += PARALLEL_BATCH) {
    const batch = dogs.slice(i, i + PARALLEL_BATCH);
    const results = await Promise.allSettled(
      batch.map(dog => verifyOneDog(dog as DogToVerify, rgClient, supabase, now))
    );
    for (let j = 0; j < results.length; j++) {
      const r = results[j];
      const dog = batch[j] as DogToVerify;
      checked++;
      if (r.status === "fulfilled") {
        const v = r.value;
        if (v.outcome === "verified") verified++;
        else if (v.outcome === "deactivated") { deactivated++; notFound++; }
        else if (v.outcome === "not_found") notFound++;
        else if (v.outcome === "pending") pending++;
        else if (v.outcome === "error") errors++;
        if (v.capturedBirthDate) birthDatesCaptured++;
        if (v.externalUrlDead) deadExternalUrls++;
        if (v.capturedAvailableDate) availableDatesCaptured++;
        if (v.capturedFoundDate) foundDatesCaptured++;
        if (v.capturedKillDate) killDatesCaptured++;
        if (v.dateCapped) datesCapped++;
        if (onDogProcessed) {
          // Stagger callbacks so dashboard slideshow shows each dog individually
          if (j > 0) await new Promise(r => setTimeout(r, 150));
          onDogProcessed({
            dog: {
              id: dog.id,
              name: dog.name,
              breed_primary: dog.breed_primary ?? null,
              primary_photo_url: dog.primary_photo_url ?? null,
              shelter_id: dog.shelter_id ?? null,
              external_id: dog.external_id,
              intake_date: dog.intake_date,
            },
            outcome: v.outcome,
            capturedBirthDate: v.capturedBirthDate,
            capturedAvailableDate: v.capturedAvailableDate,
            capturedFoundDate: v.capturedFoundDate,
            capturedKillDate: v.capturedKillDate,
            externalUrlDead: v.externalUrlDead,
          });
        }
      } else {
        errors++;
        if (onDogProcessed) {
          onDogProcessed({
            dog: {
              id: dog.id,
              name: dog.name,
              breed_primary: dog.breed_primary ?? null,
              primary_photo_url: dog.primary_photo_url ?? null,
              shelter_id: dog.shelter_id ?? null,
              external_id: dog.external_id,
              intake_date: dog.intake_date,
            },
            outcome: "error",
            capturedBirthDate: false,
            capturedAvailableDate: false,
            capturedFoundDate: false,
            capturedKillDate: false,
            externalUrlDead: false,
            error: r.reason?.message || "Unknown error",
          });
        }
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
    deadExternalUrls,
    availableDatesCaptured,
    foundDatesCaptured,
    killDatesCaptured,
    datesCapped,
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
): Promise<{ outcome: VerifyOutcome; capturedBirthDate: boolean; externalUrlDead: boolean; capturedAvailableDate: boolean; capturedFoundDate: boolean; capturedKillDate: boolean; dateCapped: boolean }> {
  const rgResult = await rgClient.verifyAnimal(dog.external_id);
  const noCapture = { capturedAvailableDate: false, capturedFoundDate: false, capturedKillDate: false, dateCapped: false };

  if (rgResult.status === "available") {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const updateData: Record<string, any> = {
      verification_status: "verified",
      last_verified_at: now,
    };

    let capturedBirthDate = false;
    let externalUrlDead = false;
    let capturedAvailableDate = false;
    let capturedFoundDate = false;
    let capturedKillDate = false;

    if (rgResult.animal) {
      const attrs = rgResult.animal.attributes as RGAnimalAttributes;

      // ─── RETURNED DOG CHECK ───
      // If adoptedDate < updatedDate, dog was adopted and returned.
      // Wait time starts from the return (updatedDate).
      const adoptedDate = attrs.adoptedDate ? new Date(attrs.adoptedDate) : null;
      const rgUpdatedDate = attrs.updatedDate ? new Date(attrs.updatedDate) : null;
      if (adoptedDate && rgUpdatedDate && !isNaN(adoptedDate.getTime()) && !isNaN(rgUpdatedDate.getTime()) && rgUpdatedDate > adoptedDate) {
        updateData.intake_date = attrs.updatedDate;
        updateData.date_confidence = "high";
        updateData.date_source = "rescuegroups_returned_after_adoption";
        capturedAvailableDate = true; // count as a date capture
      }

      // ─── Capture availableDate (GOLD STANDARD for wait time) ───
      if (!updateData.date_source && attrs.availableDate) {
        const ad = new Date(attrs.availableDate);
        const nowDate2 = new Date();
        if (!isNaN(ad.getTime()) && ad <= nowDate2) {
          updateData.intake_date = attrs.availableDate;
          updateData.date_confidence = "verified";
          updateData.date_source = "rescuegroups_available_date";
          capturedAvailableDate = true;
        }
      }

      // ─── Capture foundDate (intake date for strays) ───
      if (!updateData.date_source && attrs.foundDate) {
        const fd = new Date(attrs.foundDate);
        const nowDate = new Date();
        if (!isNaN(fd.getTime()) && fd <= nowDate) {
          updateData.intake_date = attrs.foundDate;
          updateData.date_confidence = "verified";
          updateData.date_source = "rescuegroups_found_date";
          capturedFoundDate = true;
        }
      }

      // NOTE: RescueGroups killDate is NOT used — unverifiable.
      // Euthanasia dates should only come from verified sources
      // (scraped at-risk pages, direct shelter feeds).
      // capturedKillDate remains false.

      // ─── Capture birthDate ───
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
          // (only if we didn't already set a verified date above)
          if (!updateData.date_confidence || updateData.date_confidence !== "verified") {
            const intakeDate = new Date(updateData.intake_date || dog.intake_date);
            if (intakeDate < bd) {
              updateData.intake_date = bd.toISOString();
              updateData.date_confidence = "low";
              updateData.date_source = `age_capped_api_birth_date${attrs.isBirthDateExact ? "_exact" : ""}`;
            }
          }
        }
      }

      // Capture courtesy listing flag
      if (attrs.isCourtesyListing !== undefined) {
        updateData.is_courtesy_listing = attrs.isCourtesyListing;
      }
    }

    const finalizeDateMetadata = () => {
      const effectiveConfidence = (updateData.date_confidence || dog.date_confidence || "unknown") as DateConfidence;
      const effectiveIntakeDate = updateData.intake_date || dog.intake_date;
      const intakeChanged = effectiveIntakeDate !== dog.intake_date;

      updateData.ranking_eligible = effectiveConfidence === "verified";

      if (intakeChanged) {
        updateData.intake_date_observation_count = 1;
        if (!dog.original_intake_date) {
          updateData.original_intake_date = dog.intake_date;
        }
        return;
      }

      if (effectiveConfidence === "verified") {
        updateData.intake_date_observation_count = (dog.intake_date_observation_count || 1) + 1;
      }
    };

    // ─── ENFORCE MAX_WAIT_DAYS caps on every verification ───
    // Even if no better date was found, check if the current wait time
    // exceeds the max for this dog's date confidence level.
    const effectiveIntakeDate = new Date(updateData.intake_date || dog.intake_date);
    const effectiveConfidence = (updateData.date_confidence || dog.date_confidence || "unknown") as DateConfidence;
    const nowDate = new Date();
    const waitDays = (nowDate.getTime() - effectiveIntakeDate.getTime()) / (1000 * 60 * 60 * 24);
    const maxDays = MAX_WAIT_DAYS[effectiveConfidence] || MAX_WAIT_DAYS.unknown;

    if (waitDays > maxDays) {
      const cappedDate = new Date(nowDate.getTime() - maxDays * 24 * 60 * 60 * 1000);
      updateData.intake_date = cappedDate.toISOString();
      updateData.date_confidence = effectiveConfidence === "verified" ? "medium" : "low";
      updateData.date_source = `${updateData.date_source || dog.date_source || "unknown"}|verify_capped_${maxDays}d`;
      capturedAvailableDate = true; // count as date improvement
    }

    // ─── Age vs wait time cross-check ───
    // If we have a birth_date and wait > age, cap to birth date
    const birthDateForCap = updateData.birth_date ? new Date(updateData.birth_date) :
      (dog.birth_date ? new Date(dog.birth_date) : null);
    if (birthDateForCap && !isNaN(birthDateForCap.getTime())) {
      const currentIntake = new Date(updateData.intake_date || dog.intake_date);
      if (currentIntake < birthDateForCap) {
        updateData.intake_date = birthDateForCap.toISOString();
        updateData.date_confidence = "low";
        updateData.date_source = `${updateData.date_source || dog.date_source || "unknown"}|verify_age_capped`;
      }
    }

    // Also check if the external URL is still alive + capture page_hash
    if (dog.external_url) {
      const urlCheck = await checkUrlAlive(dog.external_url, { fetchBody: true });
      if (!urlCheck.alive) externalUrlDead = true;
      if (urlCheck.pageHash) {
        updateData.page_hash = urlCheck.pageHash;
      }
    }

    const dateCapped = waitDays > maxDays;

    // Append verification event to source_links for audit trail
    const existingLinks: SourceLink[] = Array.isArray(dog.source_links) ? dog.source_links : [];
    const verificationEntry: SourceLink = {
      url: `https://www.rescuegroups.org/animals/${dog.external_id}`,
      source: "Verification Check",
      checked_at: now,
      status_code: 200,
      description: `Verified available via RescueGroups API${capturedBirthDate ? " (birthDate captured)" : ""}${capturedAvailableDate ? " (availableDate captured)" : ""}`,
    };
    // Keep max 20 verification entries to prevent unbounded growth
    const verificationLinks = existingLinks.filter(l => l.source === "Verification Check");
    const nonVerificationLinks = existingLinks.filter(l => l.source !== "Verification Check");
    const cappedVerificationLinks = [...verificationLinks.slice(-19), verificationEntry];
    updateData.source_links = [...nonVerificationLinks, ...cappedVerificationLinks];

    finalizeDateMetadata();

    await supabase.from("dogs").update(updateData).eq("id", dog.id);
    return { outcome: "verified", capturedBirthDate, externalUrlDead, capturedAvailableDate, capturedFoundDate, capturedKillDate, dateCapped };
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
    return { outcome: "pending", capturedBirthDate: false, externalUrlDead: false, capturedAvailableDate: false, capturedFoundDate: false, capturedKillDate: false, dateCapped: false };
  }

  if (rgResult.status === "api_error") {
    return { outcome: "error", capturedBirthDate: false, externalUrlDead: false, capturedAvailableDate: false, capturedFoundDate: false, capturedKillDate: false, dateCapped: false };
  }

  // Status is "not_found" — dog no longer in RescueGroups
  let urlAlive = false;
  if (dog.external_url) {
    const urlCheck = await checkUrlAlive(dog.external_url);
    urlAlive = urlCheck.alive;
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
    return { outcome: "deactivated", capturedBirthDate: false, externalUrlDead: true, capturedAvailableDate: false, capturedFoundDate: false, capturedKillDate: false, dateCapped: false };
  } else {
    await supabase
      .from("dogs")
      .update({
        verification_status: "not_found",
        last_verified_at: now,
      })
      .eq("id", dog.id);
    return { outcome: "not_found", capturedBirthDate: false, externalUrlDead: false, capturedAvailableDate: false, capturedFoundDate: false, capturedKillDate: false, dateCapped: false };
  }
}

/**
 * Check if a URL is still live (returns 200-399).
 * Optionally fetches body content for page_hash change detection.
 */
async function checkUrlAlive(
  url: string,
  options?: { fetchBody?: boolean }
): Promise<{ alive: boolean; pageHash?: string }> {
  try {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 5000);

    const method = options?.fetchBody ? "GET" : "HEAD";
    const response = await fetch(url, {
      method,
      signal: controller.signal,
      redirect: "follow",
      headers: {
        "User-Agent": "WaitingTheLongest.com/1.0 (dog-adoption-verification)",
      },
    });

    clearTimeout(timeout);
    const alive = response.status >= 200 && response.status < 400;

    let pageHash: string | undefined;
    if (alive && options?.fetchBody) {
      try {
        const body = await response.text();
        // Strip volatile elements (timestamps, CSRF tokens) for stable hashing
        const stable = body
          .replace(/<script[^>]*>[\s\S]*?<\/script>/gi, "")
          .replace(/csrf[^"']*["'][^"']*["']/gi, "")
          .replace(/\d{10,13}/g, ""); // strip unix timestamps
        pageHash = await computePageHash(stable);
      } catch {
        // Body read failed, still return alive status
      }
    }

    return { alive, pageHash };
  } catch {
    return { alive: false };
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
