/**
 * Bulk Re-Process Dates Endpoint
 *
 * Targets dogs stuck on weak date sources (rescuegroups_updated_date, etc.)
 * and re-queries the RescueGroups individual animal endpoint to capture
 * availableDate, foundDate, killDate, and adoptedDate.
 *
 * The search endpoint does NOT return these fields — only the individual
 * animal endpoint does. So this is the only way to backfill gold-standard
 * dates for existing dogs.
 *
 * Usage:
 *   GET /api/reprocess-dates?batch=100
 *   GET /api/reprocess-dates?source=rescuegroups_updated_date&batch=200
 *
 * Requires CRON_SECRET authorization.
 */
import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { createRescueGroupsClient, type RGAnimalAttributes } from "@/lib/rescuegroups/client";
import { getStrayHoldDays } from "@/data/stray-hold-days";

export const runtime = "nodejs";
export const maxDuration = 300; // 5 minutes

// Date sources ranked from weakest to strongest
const WEAK_SOURCES = [
  "rescuegroups_updated_date",
  "rescuegroups_updated_date_stale",
  "rescuegroups_updated_date_only",
  "audit_repair_stale_date",
  "no_date_available",
  "rescuegroups_created_date",
  "rescuegroups_created_date_active",
  "rescuegroups_courtesy_listing_created_date",
];

export async function GET(request: Request) {
  const authHeader = request.headers.get("authorization");
  const cronSecret = process.env.CRON_SECRET;
  if (!cronSecret || authHeader !== `Bearer ${cronSecret}`) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const url = new URL(request.url);
  const requestedBatch = parseInt(url.searchParams.get("batch") || "100", 10);
  const batchSize = Math.min(Math.max(requestedBatch || 100, 1), 500);
  const targetSource = url.searchParams.get("source") || null;

  if (targetSource && !WEAK_SOURCES.includes(targetSource)) {
    return NextResponse.json(
      { error: "Unsupported source filter", supported_sources: WEAK_SOURCES },
      { status: 400 }
    );
  }

  const startTime = Date.now();
  const supabase = createAdminClient();
  const rgClient = createRescueGroupsClient();

  let processed = 0;
  let upgraded = 0;
  let availableDates = 0;
  let foundDates = 0;
  let killDates = 0;
  let returnedDogs = 0;
  let birthDates = 0;
  let apiErrors = 0;
  let notFound = 0;
  let noImprovement = 0;

  // Fetch dogs with weak date sources, prioritizing the weakest first
  let query = supabase
    .from("dogs")
    .select("id, name, shelter_id, external_id, intake_date, date_source, date_confidence, birth_date, age_months, original_intake_date, intake_date_observation_count")
    .eq("is_available", true)
    .eq("external_source", "rescuegroups")
    .not("external_id", "is", null)
    .limit(batchSize)
    .order("intake_date", { ascending: true }); // oldest first = most impactful

  if (targetSource) {
    query = query.eq("date_source", targetSource);
  } else {
    // Target all weak sources
    query = query.in("date_source", WEAK_SOURCES);
  }

  const { data: dogs, error: fetchError } = await query;

  if (fetchError || !dogs || dogs.length === 0) {
    return NextResponse.json({
      success: true,
      message: "No dogs to reprocess",
      processed: 0,
      duration_ms: Date.now() - startTime,
    });
  }

  const shelterIds = Array.from(new Set(dogs.map((dog) => dog.shelter_id).filter(Boolean)));
  const shelterMap = new Map<string, { shelter_type: string | null; state_code: string | null; stray_hold_days: number | null }>();

  if (shelterIds.length > 0) {
    const { data: shelters } = await supabase
      .from("shelters")
      .select("id, shelter_type, state_code, stray_hold_days")
      .in("id", shelterIds);

    for (const shelter of shelters || []) {
      shelterMap.set(shelter.id, {
        shelter_type: shelter.shelter_type || null,
        state_code: shelter.state_code || null,
        stray_hold_days: shelter.stray_hold_days ?? null,
      });
    }
  }

  const now = new Date().toISOString();
  const PARALLEL_BATCH = 5;

  for (let i = 0; i < dogs.length; i += PARALLEL_BATCH) {
    const batch = dogs.slice(i, i + PARALLEL_BATCH);
    const results = await Promise.allSettled(
      batch.map(async (dog) => {
        const result = await rgClient.verifyAnimal(dog.external_id);
        return { dog, result };
      })
    );

    for (const r of results) {
      processed++;

      if (r.status === "rejected") {
        apiErrors++;
        continue;
      }

      const { dog, result } = r.value;

      if (result.status === "api_error") {
        apiErrors++;
        continue;
      }

      if (result.status === "not_found") {
        // Dog no longer on RG — mark it
        await supabase.from("dogs").update({
          verification_status: "not_found",
          last_verified_at: now,
          is_available: false,
          status: "adopted",
        }).eq("id", dog.id);
        notFound++;
        continue;
      }

      if (!result.animal) {
        noImprovement++;
        continue;
      }

      const attrs = result.animal.attributes as RGAnimalAttributes;
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      const updateData: Record<string, any> = {
        verification_status: "verified",
        last_verified_at: now,
      };

      let didUpgrade = false;
      const shelterInfo = dog.shelter_id ? shelterMap.get(dog.shelter_id) : null;

      const applyVerifiedDateMetadata = () => {
        updateData.ranking_eligible = true;
        updateData.intake_date_observation_count = 1;
        if (!dog.original_intake_date) {
          updateData.original_intake_date = dog.intake_date;
        }
      };

      // ── RETURNED DOG CHECK ──
      const adoptedDate = attrs.adoptedDate ? new Date(attrs.adoptedDate) : null;
      const rgUpdatedDate = attrs.updatedDate ? new Date(attrs.updatedDate) : null;
      if (adoptedDate && rgUpdatedDate && !isNaN(adoptedDate.getTime()) && !isNaN(rgUpdatedDate.getTime()) && rgUpdatedDate > adoptedDate) {
        updateData.intake_date = attrs.updatedDate;
        updateData.date_confidence = "high";
        updateData.date_source = "rescuegroups_returned_after_adoption";
        returnedDogs++;
        didUpgrade = true;
      }

      // ── AVAILABLE DATE (GOLD STANDARD) ──
      if (!didUpgrade && attrs.availableDate) {
        const ad = new Date(attrs.availableDate);
        const nowDate = new Date();
        if (!isNaN(ad.getTime()) && ad <= nowDate) {
          let intakeDate = attrs.availableDate;
          let dateSource = "rescuegroups_available_date";

          if (shelterInfo?.shelter_type === "municipal" && shelterInfo.state_code) {
            const holdDays = getStrayHoldDays(shelterInfo.state_code, shelterInfo.stray_hold_days);
            const adjustedDate = new Date(ad.getTime() - holdDays * 86400000);
            intakeDate = adjustedDate.toISOString();
            dateSource += `|stray_hold_adjusted_${holdDays}d`;
          }

          updateData.intake_date = intakeDate;
          updateData.date_confidence = "verified";
          updateData.date_source = dateSource;
          applyVerifiedDateMetadata();
          availableDates++;
          didUpgrade = true;
        }
      }

      // ── FOUND DATE (strays) ──
      if (!didUpgrade && attrs.foundDate) {
        const fd = new Date(attrs.foundDate);
        const nowDate = new Date();
        if (!isNaN(fd.getTime()) && fd <= nowDate) {
          updateData.intake_date = attrs.foundDate;
          updateData.date_confidence = "verified";
          updateData.date_source = "rescuegroups_found_date";
          applyVerifiedDateMetadata();
          foundDates++;
          didUpgrade = true;
        }
      }

      // NOTE: RescueGroups killDate is NOT used — unverifiable.
      // Euthanasia dates should only come from verified sources.

      // ── BIRTH DATE capture ──
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
          birthDates++;

          // Age sanity: if intake_date is before birth, cap it
          if (!updateData.date_confidence || updateData.date_confidence !== "verified") {
            const intakeDate = new Date(updateData.intake_date || dog.intake_date);
            if (intakeDate < bd) {
              updateData.intake_date = bd.toISOString();
              updateData.date_confidence = "low";
              updateData.date_source = `age_capped_api_birth_date${attrs.isBirthDateExact ? "_exact" : ""}`;
              updateData.ranking_eligible = false;
              updateData.intake_date_observation_count = 1;
              didUpgrade = true;
            }
          }
        }
      }

      // ── Courtesy listing flag ──
      if (attrs.isCourtesyListing !== undefined) {
        updateData.is_courtesy_listing = attrs.isCourtesyListing;
      }

      // ── If no gold-standard date found, at least upgrade from updatedDate to createdDate ──
      if (!didUpgrade && attrs.createdDate && dog.date_source?.includes("updated_date")) {
        const cd = new Date(attrs.createdDate);
        const nowDate = new Date();
        if (!isNaN(cd.getTime()) && cd <= nowDate) {
          const daysSince = (nowDate.getTime() - cd.getTime()) / (1000 * 60 * 60 * 24);
          updateData.intake_date = attrs.createdDate;
          updateData.date_source = `rescuegroups_created_date${attrs.updatedDate && (nowDate.getTime() - new Date(attrs.updatedDate).getTime()) / (1000 * 60 * 60 * 24) < 180 ? "_active" : ""}`;
          if (daysSince <= 90) updateData.date_confidence = "high";
          else if (daysSince <= 365) updateData.date_confidence = "high";
          else if (daysSince <= 730) updateData.date_confidence = "medium";
          else updateData.date_confidence = "low";
          updateData.ranking_eligible = false;
          updateData.intake_date_observation_count = 1;
          didUpgrade = true;
        }
      }

      if (didUpgrade) upgraded++;
      else noImprovement++;

      await supabase.from("dogs").update(updateData).eq("id", dog.id);
    }
  }

  const duration = Date.now() - startTime;

  return NextResponse.json({
    success: true,
    message: `Reprocessed ${processed} dogs: ${upgraded} upgraded, ${notFound} removed, ${noImprovement} unchanged`,
    stats: {
      processed,
      upgraded,
      availableDates,
      foundDates,
      killDates,
      returnedDogs,
      birthDates,
      apiErrors,
      notFound,
      noImprovement,
    },
    duration_ms: duration,
    rate: `${(processed / (duration / 1000)).toFixed(1)} dogs/sec`,
  });
}
