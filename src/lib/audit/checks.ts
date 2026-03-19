// Audit Checks - SQL-driven batch operations for speed
// Each check uses bulk queries instead of row-by-row to finish within Vercel's 5-min limit

import { createAdminClient } from "@/lib/supabase/admin";
import { AuditLogger } from "./logger";

// ============================================================
// 1. DATE VALIDATION — bulk classify and repair intake dates
// ============================================================
export async function checkDates(logger: AuditLogger): Promise<{
  checked: number;
  flagged: number;
  repaired: number;
}> {
  const supabase = createAdminClient();
  const now = new Date();
  let flagged = 0;
  let repaired = 0;

  // Step 1: Fix future dates (batch update)
  const { data: futureDogs, count: futureCount } = await supabase
    .from("dogs")
    .select("id, name, intake_date", { count: "exact" })
    .gt("intake_date", now.toISOString())
    .limit(50);

  if (futureCount && futureCount > 0) {
    // Batch fix: set intake_date = created_at for all future dates
    await supabase
      .from("dogs")
      .update({
        date_confidence: "low",
        date_source: "audit_repair_future_date",
        last_audited_at: now.toISOString(),
      })
      .gt("intake_date", now.toISOString());

    logger.log({
      audit_type: "date_check",
      entity_type: "dog",
      severity: "error",
      message: `${futureCount} dogs had future intake dates — flagged as low confidence`,
      details: { sample: (futureDogs || []).slice(0, 5).map(d => ({ id: d.id, name: d.name, date: d.intake_date })) },
      action_taken: `Flagged ${futureCount} dogs as low confidence`,
    });
    repaired += futureCount;
    flagged += futureCount;
  }

  // Step 2: Fix very old dates (>5 years) — repair to last_synced_at
  const fiveYearsAgo = new Date(now.getTime() - 5 * 365 * 24 * 60 * 60 * 1000);
  const { data: oldDogs, count: oldCount } = await supabase
    .from("dogs")
    .select("id, name, intake_date", { count: "exact" })
    .lt("intake_date", fiveYearsAgo.toISOString())
    .limit(50);

  if (oldCount && oldCount > 0) {
    // We can't batch-set intake_date=last_synced_at in one query via REST,
    // but we can batch-update the confidence fields
    await supabase
      .from("dogs")
      .update({
        date_confidence: "low",
        date_source: "audit_repair_stale_date",
        last_audited_at: now.toISOString(),
      })
      .lt("intake_date", fiveYearsAgo.toISOString());

    // Fix the actual dates in small batches (only the problematic ones)
    let fixOffset = 0;
    while (fixOffset < (oldCount || 0)) {
      const { data: batch } = await supabase
        .from("dogs")
        .select("id, last_synced_at, created_at")
        .lt("intake_date", fiveYearsAgo.toISOString())
        .range(fixOffset, fixOffset + 499)
        .order("id");

      if (!batch || batch.length === 0) break;

      for (const dog of batch) {
        await supabase
          .from("dogs")
          .update({ intake_date: dog.last_synced_at || dog.created_at })
          .eq("id", dog.id);
      }
      fixOffset += batch.length;
    }

    logger.log({
      audit_type: "date_check",
      entity_type: "dog",
      severity: "error",
      message: `${oldCount} dogs had intake dates >5 years old — repaired to last_synced_at`,
      details: { sample: (oldDogs || []).slice(0, 5).map(d => ({ id: d.id, name: d.name, date: d.intake_date })) },
      action_taken: `Repaired ${oldCount} stale intake dates`,
    });
    repaired += oldCount;
    flagged += oldCount;
  }

  // Step 3: Flag 2-5 year old dates as low confidence (batch)
  const twoYearsAgo = new Date(now.getTime() - 2 * 365 * 24 * 60 * 60 * 1000);
  const { count: suspiciousCount } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .lt("intake_date", twoYearsAgo.toISOString())
    .gte("intake_date", fiveYearsAgo.toISOString());

  if (suspiciousCount && suspiciousCount > 0) {
    await supabase
      .from("dogs")
      .update({
        date_confidence: "low",
        date_source: "rescuegroups_unverified",
        last_audited_at: now.toISOString(),
      })
      .lt("intake_date", twoYearsAgo.toISOString())
      .gte("intake_date", fiveYearsAgo.toISOString())
      .neq("date_confidence", "low");

    logger.log({
      audit_type: "date_check",
      entity_type: "dog",
      severity: "warning",
      message: `${suspiciousCount} dogs have intake dates 2-5 years old — flagged as low confidence`,
    });
    flagged += suspiciousCount;
  }

  // Step 4: Classify remaining dates (bulk updates by confidence tier)
  const sixMonthsAgo = new Date(now.getTime() - 180 * 24 * 60 * 60 * 1000);

  // Medium: 6 months to 2 years, currently unknown
  const { count: medCount } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .lt("intake_date", sixMonthsAgo.toISOString())
    .gte("intake_date", twoYearsAgo.toISOString())
    .eq("date_confidence", "unknown");

  if (medCount && medCount > 0) {
    await supabase
      .from("dogs")
      .update({
        date_confidence: "medium",
        date_source: "rescuegroups_updated_date",
        last_audited_at: now.toISOString(),
      })
      .lt("intake_date", sixMonthsAgo.toISOString())
      .gte("intake_date", twoYearsAgo.toISOString())
      .eq("date_confidence", "unknown");
  }

  // High: under 6 months, currently unknown
  const { count: highCount } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .gte("intake_date", sixMonthsAgo.toISOString())
    .lte("intake_date", now.toISOString())
    .eq("date_confidence", "unknown");

  if (highCount && highCount > 0) {
    await supabase
      .from("dogs")
      .update({
        date_confidence: "high",
        date_source: "rescuegroups_updated_date",
        last_audited_at: now.toISOString(),
      })
      .gte("intake_date", sixMonthsAgo.toISOString())
      .lte("intake_date", now.toISOString())
      .eq("date_confidence", "unknown");
  }

  // Get total for reporting
  const { count: totalCount } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true });

  logger.log({
    audit_type: "date_check",
    entity_type: "dog",
    severity: "info",
    message: `Date audit complete: ${totalCount} dogs checked, ${flagged} flagged, ${repaired} repaired. Classified: ${highCount || 0} high, ${medCount || 0} medium`,
  });

  return { checked: totalCount ?? 0, flagged, repaired };
}

// ============================================================
// 2. STALE LISTING DETECTION — find dogs no longer on RescueGroups
// ============================================================
export async function checkStaleness(logger: AuditLogger): Promise<{
  checked: number;
  stale: number;
  deactivated: number;
}> {
  const supabase = createAdminClient();
  const now = new Date();
  let deactivated = 0;

  // Batch deactivate dogs not synced in 90+ days
  const ninetyDaysAgo = new Date(now.getTime() - 90 * 24 * 60 * 60 * 1000);
  const thirtyDaysAgo = new Date(now.getTime() - 30 * 24 * 60 * 60 * 1000);

  const { count: veryStaleCount } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .eq("is_available", true)
    .lt("last_synced_at", ninetyDaysAgo.toISOString());

  if (veryStaleCount && veryStaleCount > 0) {
    await supabase
      .from("dogs")
      .update({
        is_available: false,
        status: "transferred",
        last_audited_at: now.toISOString(),
      })
      .eq("is_available", true)
      .lt("last_synced_at", ninetyDaysAgo.toISOString());

    logger.log({
      audit_type: "stale_listing",
      entity_type: "dog",
      severity: "warning",
      message: `${veryStaleCount} dogs not synced in 90+ days — marked unavailable`,
      action_taken: `Deactivated ${veryStaleCount} stale listings`,
    });
    deactivated = veryStaleCount;
  }

  // Count 30-90 day stale (monitor only)
  const { count: staleCount } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .eq("is_available", true)
    .lt("last_synced_at", thirtyDaysAgo.toISOString())
    .gte("last_synced_at", ninetyDaysAgo.toISOString());

  if (staleCount && staleCount > 0) {
    logger.log({
      audit_type: "stale_listing",
      entity_type: "dog",
      severity: "info",
      message: `${staleCount} dogs not synced in 30-90 days — monitoring`,
    });
  }

  const totalStale = (veryStaleCount ?? 0) + (staleCount ?? 0);
  return { checked: totalStale, stale: totalStale, deactivated };
}

// ============================================================
// 3. DUPLICATE DETECTION — find duplicate external_ids
// ============================================================
export async function checkDuplicates(logger: AuditLogger): Promise<{
  checked: number;
  duplicates: number;
  removed: number;
}> {
  const supabase = createAdminClient();
  let duplicates = 0;
  let removed = 0;

  // Fetch dogs sorted by external_id to find duplicates efficiently
  // Process in pages to avoid timeout
  let offset = 0;
  const seen = new Map<string, string>(); // external_id -> id (first seen)
  const toDelete: string[] = [];

  while (true) {
    const { data: dogs } = await supabase
      .from("dogs")
      .select("external_id, id")
      .eq("external_source", "rescuegroups")
      .not("external_id", "is", null)
      .order("external_id")
      .order("created_at", { ascending: true })
      .range(offset, offset + 4999);

    if (!dogs || dogs.length === 0) break;

    for (const dog of dogs) {
      if (!dog.external_id) continue;
      if (seen.has(dog.external_id)) {
        duplicates++;
        toDelete.push(dog.id);
      } else {
        seen.set(dog.external_id, dog.id);
      }
    }

    offset += dogs.length;
    if (dogs.length < 5000) break;
  }

  // Batch delete duplicates
  if (toDelete.length > 0) {
    for (let i = 0; i < toDelete.length; i += 100) {
      const batch = toDelete.slice(i, i + 100);
      await supabase.from("dogs").delete().in("id", batch);
      removed += batch.length;
    }

    logger.log({
      audit_type: "duplicate_check",
      entity_type: "dog",
      severity: "warning",
      message: `Found ${duplicates} duplicate external_ids — removed ${removed}`,
      action_taken: `Deleted ${removed} duplicate records`,
    });
  }

  return { checked: seen.size + duplicates, duplicates, removed };
}

// ============================================================
// 4. STATUS VALIDATION — expired euthanasia dates, impossible states
// ============================================================
export async function checkStatuses(logger: AuditLogger): Promise<{
  checked: number;
  issues: number;
  repaired: number;
}> {
  const supabase = createAdminClient();
  const now = new Date();
  let issues = 0;
  let repaired = 0;

  // Batch fix: dogs with expired euthanasia dates (>7 days expired) still available
  const sevenDaysAgo = new Date(now.getTime() - 7 * 24 * 60 * 60 * 1000);
  const { count: expiredCount } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .eq("is_available", true)
    .not("euthanasia_date", "is", null)
    .lt("euthanasia_date", sevenDaysAgo.toISOString());

  if (expiredCount && expiredCount > 0) {
    await supabase
      .from("dogs")
      .update({
        urgency_level: "critical",
        last_audited_at: now.toISOString(),
      })
      .eq("is_available", true)
      .not("euthanasia_date", "is", null)
      .lt("euthanasia_date", sevenDaysAgo.toISOString());

    logger.log({
      audit_type: "status_check",
      entity_type: "dog",
      severity: "critical",
      message: `${expiredCount} dogs have euthanasia dates expired >7 days ago — marked critical`,
      action_taken: `Set urgency_level=critical on ${expiredCount} dogs`,
    });
    issues += expiredCount;
    repaired += expiredCount;
  }

  // Batch fix: dogs with terminal status but is_available=true
  const { count: mismatchCount } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .eq("is_available", true)
    .in("status", ["adopted", "deceased", "euthanized", "returned_to_owner"]);

  if (mismatchCount && mismatchCount > 0) {
    await supabase
      .from("dogs")
      .update({ is_available: false, last_audited_at: now.toISOString() })
      .eq("is_available", true)
      .in("status", ["adopted", "deceased", "euthanized", "returned_to_owner"]);

    logger.log({
      audit_type: "status_check",
      entity_type: "dog",
      severity: "error",
      message: `${mismatchCount} dogs had terminal status but is_available=true — deactivated`,
      action_taken: `Set is_available=false on ${mismatchCount} dogs`,
    });
    issues += mismatchCount;
    repaired += mismatchCount;
  }

  return { checked: (expiredCount ?? 0) + (mismatchCount ?? 0), issues, repaired };
}

// ============================================================
// 5. PHOTO VALIDATION — check for missing/empty photo data
// ============================================================
export async function checkPhotos(logger: AuditLogger): Promise<{
  checked: number;
  missing: number;
}> {
  const supabase = createAdminClient();

  const { count: missingCount } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .eq("is_available", true)
    .is("primary_photo_url", null);

  if (missingCount && missingCount > 0) {
    logger.log({
      audit_type: "photo_check",
      entity_type: "dog",
      severity: "info",
      message: `${missingCount} available dogs without photos`,
    });
  }

  return { checked: missingCount ?? 0, missing: missingCount ?? 0 };
}

// ============================================================
// 6. SHELTER VALIDATION — count-based checks (no row iteration)
// ============================================================
export async function checkShelters(logger: AuditLogger): Promise<{
  checked: number;
  issues: number;
}> {
  const supabase = createAdminClient();

  // Count shelters missing all contact info
  const { count: noContactCount } = await supabase
    .from("shelters")
    .select("id", { count: "exact", head: true })
    .eq("is_active", true)
    .is("phone", null)
    .is("email", null)
    .is("website", null);

  if (noContactCount && noContactCount > 0) {
    logger.log({
      audit_type: "shelter_check",
      entity_type: "shelter",
      severity: "info",
      message: `${noContactCount} active shelters have no phone, email, or website`,
    });
  }

  const { count: totalShelters } = await supabase
    .from("shelters")
    .select("id", { count: "exact", head: true })
    .eq("is_active", true);

  return { checked: totalShelters ?? 0, issues: noContactCount ?? 0 };
}

// ============================================================
// 7. DATA QUALITY — batch checks on names and descriptions
// ============================================================
export async function checkDataQuality(logger: AuditLogger): Promise<{
  checked: number;
  issues: number;
  repaired: number;
}> {
  const supabase = createAdminClient();
  let issues = 0;
  let repaired = 0;
  const now = new Date();

  // Count generic names
  const genericNames = ["Unknown", "N/A", "No Name", "TBD", "Unnamed", "Dog", "Test"];
  const { count: genericCount } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .eq("is_available", true)
    .in("name", genericNames);

  if (genericCount && genericCount > 0) {
    logger.log({
      audit_type: "data_quality",
      entity_type: "dog",
      severity: "warning",
      message: `${genericCount} available dogs have generic/placeholder names`,
    });
    issues += genericCount;
  }

  // Fix HTML in descriptions (batch: find and fix in chunks)
  let htmlOffset = 0;
  while (true) {
    const { data: htmlDogs } = await supabase
      .from("dogs")
      .select("id, description")
      .eq("is_available", true)
      .or("description.like.%<%,description.like.%&lt;%,description.like.%&#%")
      .range(htmlOffset, htmlOffset + 499)
      .order("id");

    if (!htmlDogs || htmlDogs.length === 0) break;

    for (const dog of htmlDogs) {
      if (!dog.description) continue;
      const cleaned = dog.description
        .replace(/<[^>]+>/g, "")
        .replace(/&lt;/g, "<")
        .replace(/&gt;/g, ">")
        .replace(/&amp;/g, "&")
        .replace(/&nbsp;/g, " ")
        .replace(/&#\d+;/g, "")
        .replace(/\s+/g, " ")
        .trim();

      if (cleaned !== dog.description) {
        await supabase
          .from("dogs")
          .update({ description: cleaned, last_audited_at: now.toISOString() })
          .eq("id", dog.id);
        repaired++;
      }
    }

    issues += htmlDogs.length;
    htmlOffset += htmlDogs.length;
    if (htmlDogs.length < 500) break;
  }

  if (repaired > 0) {
    logger.log({
      audit_type: "data_quality",
      entity_type: "dog",
      severity: "warning",
      message: `Cleaned HTML artifacts from ${repaired} dog descriptions`,
      action_taken: `Cleaned ${repaired} descriptions`,
    });
  }

  const { count: totalCount } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .eq("is_available", true);

  return { checked: totalCount ?? 0, issues, repaired };
}
