/**
 * Persist credibility_score on the dogs table.
 *
 * The score is computed at read time by computeCredibilityScore(), but we
 * also want it persisted so queries can filter/sort by it (e.g. "show me
 * dogs with score >= 60"). This module computes and writes the stored
 * column for a batch of dogs.
 *
 * Intended to run:
 *   - at the end of each sync job (covers fresh/updated dogs)
 *   - in the daily audit cron (covers stragglers)
 */

import type { SupabaseClient } from "@supabase/supabase-js";
import { computeCredibilityScore } from "./credibility-score";
import type { SourceLink } from "@/types/dog";
import { logger } from "./logger";

interface ScoringRow {
  id: string;
  source_links: SourceLink[] | null;
  date_confidence: string | null;
  verification_status: string | null;
  last_verified_at: string | null;
  source_nonprofit_verified: boolean | null;
  external_source: string | null;
  intake_date_observation_count: number | null;
  credibility_score: number | null;
}

/**
 * Recompute credibility_score for dogs whose stored value is stale.
 *
 * @param supabase - admin client
 * @param limit - max dogs to process in one batch
 * @returns number of dogs updated
 */
export async function updateCredibilityScores(
  supabase: SupabaseClient,
  limit = 5000
): Promise<{ scanned: number; updated: number; errors: number }> {
  const log = logger.child({ module: "persist_credibility" });
  log.info("credibility_update_start", { limit });

  // Pull a batch — prioritize dogs that have never had a score computed,
  // then dogs updated recently, then everything else.
  const { data: dogs, error } = await supabase
    .from("dogs")
    .select(
      "id, source_links, date_confidence, verification_status, last_verified_at, source_nonprofit_verified, external_source, intake_date_observation_count, credibility_score"
    )
    .eq("is_available", true)
    .order("credibility_score", { ascending: true, nullsFirst: true })
    .order("updated_at", { ascending: false })
    .limit(limit);

  if (error || !dogs) {
    log.error("credibility_fetch_failed", { error });
    return { scanned: 0, updated: 0, errors: 1 };
  }

  let updated = 0;
  let errors = 0;

  for (const row of dogs as ScoringRow[]) {
    const breakdown = computeCredibilityScore({
      source_links: row.source_links,
      date_confidence: row.date_confidence,
      verification_status: row.verification_status,
      last_verified_at: row.last_verified_at,
      source_nonprofit_verified: row.source_nonprofit_verified ?? false,
      external_source: row.external_source,
      intake_date_observation_count: row.intake_date_observation_count ?? 1,
    });

    // Only write if changed (saves write bandwidth on already-scored dogs)
    if (row.credibility_score === breakdown.total) continue;

    const { error: updErr } = await supabase
      .from("dogs")
      .update({ credibility_score: breakdown.total })
      .eq("id", row.id);

    if (updErr) {
      errors++;
    } else {
      updated++;
    }
  }

  log.info("credibility_update_done", {
    scanned: dogs.length,
    updated,
    errors,
  });

  return { scanned: dogs.length, updated, errors };
}
