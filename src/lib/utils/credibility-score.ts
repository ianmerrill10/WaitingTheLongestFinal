import type { SourceLink } from "@/types/dog";

export interface CredibilityBreakdown {
  total: number;
  sourceLinks: number;
  dateConfidence: number;
  verificationStatus: number;
  verificationRecency: number;
  shelterVerified: number;
  sourceType: number;
}

/**
 * Compute a 0-100 credibility score for a dog's data provenance.
 *
 * Breakdown (weights sum to 100):
 *  - Working source links:    25 pts (5 per link with 2xx status, max 25)
 *  - Date confidence:         20 pts (verified=20, high=15, medium=10, low=5, unknown=0)
 *  - Verification status:     20 pts (verified=20, pending=10, unverified=5, not_found=0)
 *  - Verification recency:    15 pts (today=15, <7d=12, <30d=8, <90d=4, >90d/never=0)
 *  - Shelter EIN verified:    10 pts
 *  - Source type:             10 pts (API=10, scraper=6, csv_import=3)
 */
export function computeCredibilityScore(dog: {
  source_links?: SourceLink[] | null;
  date_confidence?: string | null;
  verification_status?: string | null;
  last_verified_at?: string | null;
  source_nonprofit_verified?: boolean;
  external_source?: string | null;
  intake_date_observation_count?: number;
}): CredibilityBreakdown {
  const links = dog.source_links || [];
  const workingLinks = links.filter(
    (l) => l.status_code !== null && l.status_code >= 200 && l.status_code < 400
  );
  // Also count links that haven't been checked yet (they still exist as references)
  const referenceLinks = links.filter((l) => l.status_code === null);
  const sourceLinks = Math.min(25, workingLinks.length * 5 + referenceLinks.length * 3);

  const dateMap: Record<string, number> = {
    verified: 20,
    high: 15,
    medium: 10,
    low: 5,
    unknown: 0,
  };
  const dateConfidence = dateMap[dog.date_confidence || "unknown"] ?? 0;

  const verifyMap: Record<string, number> = {
    verified: 20,
    pending: 10,
    unverified: 5,
    not_found: 0,
  };
  const verificationStatus =
    verifyMap[dog.verification_status || "unverified"] ?? 5;

  let verificationRecency = 0;
  if (dog.last_verified_at) {
    const daysSince =
      (Date.now() - new Date(dog.last_verified_at).getTime()) / 86400000;
    if (daysSince < 1) verificationRecency = 15;
    else if (daysSince < 7) verificationRecency = 12;
    else if (daysSince < 30) verificationRecency = 8;
    else if (daysSince < 90) verificationRecency = 4;
  }

  const shelterVerified = dog.source_nonprofit_verified ? 10 : 0;

  let sourceType = 3;
  if (dog.external_source === "gov_opendata") sourceType = 10; // Gold standard
  else if (dog.external_source === "rescuegroups") sourceType = 8;
  else if (dog.external_source?.startsWith("scraper_")) sourceType = 6;

  // Crawl consistency bonus: same date seen across 2+ crawls adds confidence
  const observationCount = dog.intake_date_observation_count || 1;
  let consistencyBonus = 0;
  if (observationCount >= 5) consistencyBonus = 5;
  else if (observationCount >= 3) consistencyBonus = 3;
  else if (observationCount >= 2) consistencyBonus = 1;

  const total =
    sourceLinks +
    dateConfidence +
    verificationStatus +
    verificationRecency +
    shelterVerified +
    sourceType +
    consistencyBonus;

  return {
    total: Math.min(100, total),
    sourceLinks,
    dateConfidence,
    verificationStatus,
    verificationRecency,
    shelterVerified,
    sourceType,
  };
}

export function getCredibilityLabel(score: number): {
  label: string;
  color: string;
  bgColor: string;
} {
  if (score >= 80)
    return { label: "Excellent", color: "text-green-700", bgColor: "bg-green-100" };
  if (score >= 60)
    return { label: "Good", color: "text-blue-700", bgColor: "bg-blue-100" };
  if (score >= 40)
    return { label: "Fair", color: "text-yellow-700", bgColor: "bg-yellow-100" };
  if (score >= 20)
    return { label: "Low", color: "text-orange-700", bgColor: "bg-orange-100" };
  return { label: "Unverified", color: "text-red-700", bgColor: "bg-red-100" };
}
