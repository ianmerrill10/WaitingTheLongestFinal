import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";

/**
 * POST /api/admin/fix-wait-times
 *
 * Batch-fix unreasonable wait times in the database:
 * 1. Cap wait times by confidence level (same logic as mapper.ts)
 * 2. Remove non-dog listings (info pages, sponsorship pages, etc.)
 * 3. Downgrade confidence for capped dates
 */
export async function POST() {
  const supabase = createAdminClient();
  const now = new Date();
  const results = {
    waitTimeCapped: 0,
    nonDogsRemoved: 0,
    errors: 0,
    details: [] as string[],
  };

  // ─── Max wait time caps (days) by confidence level ───
  const MAX_WAIT_DAYS: Record<string, number> = {
    verified: 4380, // 12 years
    high: 4380,     // 12 years
    medium: 2555,   // 7 years
    low: 730,       // 2 years
    unknown: 365,   // 1 year
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
      .select("id, name, intake_date, date_source")
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
          date_confidence: newConfidence,
          date_source: `${dog.date_source || "unknown"}|wait_capped_${maxDays}d`,
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

  return NextResponse.json({
    success: true,
    ...results,
    summary: `Capped ${results.waitTimeCapped} wait times, removed ${results.nonDogsRemoved} non-dog listings, ${results.errors} errors`,
  });
}
