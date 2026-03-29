import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { MAX_WAIT_DAYS } from "@/lib/rescuegroups/mapper";

/**
 * POST /api/admin/fix-wait-times
 *
 * Batch-fix unreasonable wait times in the database:
 * 1. Cap wait times by confidence level (uses MAX_WAIT_DAYS from mapper)
 * 2. Remove non-dog listings (info pages, sponsorship pages, etc.)
 * 3. Downgrade confidence for capped dates
 * 4. Downgrade old created_date sources to low confidence
 */
export async function POST() {
  const supabase = createAdminClient();
  const now = new Date();
  const results = {
    waitTimeCapped: 0,
    nonDogsRemoved: 0,
    confidenceDowngraded: 0,
    errors: 0,
    details: [] as string[],
  };

  // ─── Non-dog listing name patterns ───
  const NON_DOG_PATTERNS = [
    /^fosters?\s+needed$/i,
    /^foster\s+(?:dog\s+)?blog$/i,
    /^(?:pre-?approval|pre-?approve|application)$/i,
    /^sponsor\b/i,
    /\bsponsorship\b/i,
    /^donate\b/i,
    /^donation/i,
    /^volunteer/i,
    /^about\s+(?:us|our)/i,
    /^how\s+to\s+(?:adopt|foster|help)/i,
    /^(?:wish\s*list|wish-?list)$/i,
    /^(?:supplies|items)\s+needed$/i,
    /^(?:info|information|faq|contact)/i,
    /^(?:event|fundraiser|gala|auction)/i,
    /^(?:memorial|in\s+memory|rainbow\s+bridge)/i,
    /^(?:happy\s+tail|success\s+stor)/i,
    /^(?:adoption\s+(?:process|info|application|fee))/i,
    /^(?:our\s+(?:dogs|mission|story|team))/i,
    /^(?:foster\s+(?:info|application|program|home))/i,
    /^(?:transport|courtesy\s+(?:post|listing))/i,
    /^FOHA\s/i,
  ];

  // ─── Step 1: Find and remove non-dog listings ───
  const { data: allDogs } = await supabase
    .from("dogs")
    .select("id, name")
    .eq("is_available", true);

  if (allDogs) {
    const nonDogIds: string[] = [];
    for (const dog of allDogs) {
      const isNonDog = NON_DOG_PATTERNS.some(p => p.test(dog.name || ""));
      if (isNonDog) {
        nonDogIds.push(dog.id);
        results.details.push(`Removed non-dog: "${dog.name}"`);
      }
    }

    if (nonDogIds.length > 0) {
      // Mark as unavailable (preserves audit trail)
      const { error } = await supabase
        .from("dogs")
        .update({ is_available: false })
        .in("id", nonDogIds);

      if (error) {
        results.errors++;
        results.details.push(`Error removing non-dogs: ${error.message}`);
      } else {
        results.nonDogsRemoved = nonDogIds.length;
      }
    }
  }

  // ─── Step 2: Cap unreasonable wait times ───
  for (const [confidence, maxDays] of Object.entries(MAX_WAIT_DAYS)) {
    const cutoffDate = new Date(now);
    cutoffDate.setDate(cutoffDate.getDate() - maxDays);
    const cappedDate = cutoffDate.toISOString();

    // Find dogs where intake_date is before the cutoff for this confidence level
    const { data: stale } = await supabase
      .from("dogs")
      .select("id, name, intake_date, original_intake_date, date_source")
      .eq("is_available", true)
      .eq("date_confidence", confidence)
      .lt("intake_date", cappedDate);

    if (!stale || stale.length === 0) continue;

    for (const dog of stale) {
      const newConfidence = confidence === "verified" ? "medium" : "low";
      const { error } = await supabase
        .from("dogs")
        .update({
          intake_date: cappedDate,
          original_intake_date: dog.original_intake_date || dog.intake_date,
          date_confidence: newConfidence,
          date_source: `${dog.date_source || "unknown"}|wait_capped_${maxDays}d`,
          ranking_eligible: false,
          intake_date_observation_count: 1,
        })
        .eq("id", dog.id);

      if (error) {
        results.errors++;
      } else {
        results.waitTimeCapped++;
        results.details.push(
          `Capped "${dog.name}": ${dog.intake_date?.substring(0, 10)} → ${cappedDate.substring(0, 10)} (was ${confidence}, now ${newConfidence})`
        );
      }
    }
  }

  // ─── Step 3: Downgrade old created_date and updated_date sources ───
  // created_date > 1 year = low confidence (listing creation ≠ intake date)
  const oneYearAgo = new Date(now.getTime() - 365 * 24 * 60 * 60 * 1000);
  const { count: oldCreatedCount } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .eq("is_available", true)
    .lt("intake_date", oneYearAgo.toISOString())
    .like("date_source", "%created_date%")
    .not("date_source", "like", "%available_date%")
    .in("date_confidence", ["high", "medium", "verified"]);

  if (oldCreatedCount && oldCreatedCount > 0) {
    await supabase
      .from("dogs")
      .update({ date_confidence: "low", ranking_eligible: false })
      .eq("is_available", true)
      .lt("intake_date", oneYearAgo.toISOString())
      .like("date_source", "%created_date%")
      .not("date_source", "like", "%available_date%")
      .in("date_confidence", ["high", "medium", "verified"]);
    results.confidenceDowngraded += oldCreatedCount;
    results.details.push(`Downgraded ${oldCreatedCount} created_date sources >1yr to low confidence`);
  }

  // updated_date > 6 months = low confidence
  const sixMonthsAgo = new Date(now.getTime() - 180 * 24 * 60 * 60 * 1000);
  const { count: oldUpdatedCount } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .eq("is_available", true)
    .lt("intake_date", sixMonthsAgo.toISOString())
    .like("date_source", "%updated_date%")
    .not("date_source", "like", "%available_date%")
    .not("date_source", "like", "%found_date%")
    .in("date_confidence", ["high", "medium"]);

  if (oldUpdatedCount && oldUpdatedCount > 0) {
    await supabase
      .from("dogs")
      .update({ date_confidence: "low", ranking_eligible: false })
      .eq("is_available", true)
      .lt("intake_date", sixMonthsAgo.toISOString())
      .like("date_source", "%updated_date%")
      .not("date_source", "like", "%available_date%")
      .not("date_source", "like", "%found_date%")
      .in("date_confidence", ["high", "medium"]);
    results.confidenceDowngraded += oldUpdatedCount;
    results.details.push(`Downgraded ${oldUpdatedCount} updated_date sources >6mo to low confidence`);
  }

  // >2 years with any non-verified source = low
  const twoYearsAgo = new Date(now.getTime() - 2 * 365 * 24 * 60 * 60 * 1000);
  const { count: veryOldCount } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .eq("is_available", true)
    .lt("intake_date", twoYearsAgo.toISOString())
    .not("date_source", "like", "%available_date%")
    .not("date_source", "like", "%found_date%")
    .neq("date_confidence", "low");

  if (veryOldCount && veryOldCount > 0) {
    await supabase
      .from("dogs")
      .update({ date_confidence: "low", ranking_eligible: false })
      .eq("is_available", true)
      .lt("intake_date", twoYearsAgo.toISOString())
      .not("date_source", "like", "%available_date%")
      .not("date_source", "like", "%found_date%")
      .neq("date_confidence", "low");
    results.confidenceDowngraded += veryOldCount;
    results.details.push(`Downgraded ${veryOldCount} non-verified sources >2yr to low confidence`);
  }

  return NextResponse.json({
    success: true,
    ...results,
    summary: `Capped ${results.waitTimeCapped} wait times, removed ${results.nonDogsRemoved} non-dog listings, downgraded ${results.confidenceDowngraded} confidence levels, ${results.errors} errors`,
  });
}
