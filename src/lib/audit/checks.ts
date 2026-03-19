// Audit Checks - modular data integrity checks
// Each check function scans a batch of records and reports findings via the logger

import { createAdminClient } from "@/lib/supabase/admin";
import { AuditLogger } from "./logger";

const BATCH_SIZE = 1000;

// ============================================================
// 1. DATE VALIDATION — flag unreliable intake_date values
// ============================================================
export async function checkDates(logger: AuditLogger): Promise<{
  checked: number;
  flagged: number;
  repaired: number;
}> {
  const supabase = createAdminClient();
  let checked = 0;
  let flagged = 0;
  let repaired = 0;
  let offset = 0;
  const now = new Date();

  while (true) {
    const { data: dogs } = await supabase
      .from("dogs")
      .select("id, name, intake_date, last_synced_at, date_confidence, date_source, created_at")
      .range(offset, offset + BATCH_SIZE - 1)
      .order("id");

    if (!dogs || dogs.length === 0) break;
    checked += dogs.length;

    for (const dog of dogs) {
      const intakeDate = new Date(dog.intake_date);
      const daysSinceIntake = (now.getTime() - intakeDate.getTime()) / (1000 * 60 * 60 * 24);

      // Future dates — clear error
      if (intakeDate > now) {
        logger.log({
          audit_type: "date_check",
          entity_type: "dog",
          entity_id: dog.id,
          severity: "error",
          message: `Future intake_date: ${dog.intake_date}`,
          details: { name: dog.name, intake_date: dog.intake_date },
          action_taken: "Set intake_date to created_at",
        });

        await supabase
          .from("dogs")
          .update({
            intake_date: dog.created_at,
            date_confidence: "low",
            date_source: "audit_repair_future_date",
            last_audited_at: now.toISOString(),
          })
          .eq("id", dog.id);
        repaired++;
        flagged++;
        continue;
      }

      // More than 5 years old — almost certainly stale RescueGroups createdDate
      if (daysSinceIntake > 365 * 5) {
        logger.log({
          audit_type: "date_check",
          entity_type: "dog",
          entity_id: dog.id,
          severity: "error",
          message: `Intake date ${Math.round(daysSinceIntake / 365)}+ years old — likely stale RescueGroups data`,
          details: { name: dog.name, intake_date: dog.intake_date, days: Math.round(daysSinceIntake) },
          action_taken: "Set intake_date to last_synced_at, confidence=low",
        });

        const fallbackDate = dog.last_synced_at || dog.created_at;
        await supabase
          .from("dogs")
          .update({
            intake_date: fallbackDate,
            date_confidence: "low",
            date_source: "audit_repair_stale_date",
            last_audited_at: now.toISOString(),
          })
          .eq("id", dog.id);
        repaired++;
        flagged++;
        continue;
      }

      // 2-5 years old — suspicious, flag but don't auto-repair
      if (daysSinceIntake > 365 * 2) {
        logger.log({
          audit_type: "date_check",
          entity_type: "dog",
          entity_id: dog.id,
          severity: "warning",
          message: `Intake date ${Math.round(daysSinceIntake / 365)}+ years old — possibly inaccurate`,
          details: { name: dog.name, intake_date: dog.intake_date, days: Math.round(daysSinceIntake) },
        });

        if (dog.date_confidence !== "low") {
          await supabase
            .from("dogs")
            .update({
              date_confidence: "low",
              date_source: dog.date_source || "rescuegroups_unverified",
              last_audited_at: now.toISOString(),
            })
            .eq("id", dog.id);
        }
        flagged++;
        continue;
      }

      // 6 months to 2 years — medium confidence
      if (daysSinceIntake > 180) {
        if (!dog.date_confidence || dog.date_confidence === "unknown") {
          await supabase
            .from("dogs")
            .update({
              date_confidence: "medium",
              date_source: dog.date_source || "rescuegroups_updated_date",
              last_audited_at: now.toISOString(),
            })
            .eq("id", dog.id);
        }
        continue;
      }

      // Under 6 months — high confidence
      if (!dog.date_confidence || dog.date_confidence === "unknown") {
        await supabase
          .from("dogs")
          .update({
            date_confidence: "high",
            date_source: dog.date_source || "rescuegroups_updated_date",
            last_audited_at: now.toISOString(),
          })
          .eq("id", dog.id);
      }
    }

    offset += dogs.length;
    // Flush logs periodically
    if (offset % 5000 === 0) await logger.flush();
  }

  return { checked, flagged, repaired };
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
  let checked = 0;
  let stale = 0;
  let deactivated = 0;
  const now = new Date();

  // Dogs not synced in 30+ days are likely no longer available
  const staleThreshold = new Date(now.getTime() - 30 * 24 * 60 * 60 * 1000);

  const { data: staleDogs } = await supabase
    .from("dogs")
    .select("id, name, last_synced_at, status, is_available")
    .eq("is_available", true)
    .lt("last_synced_at", staleThreshold.toISOString());

  if (!staleDogs) return { checked: 0, stale: 0, deactivated: 0 };
  checked = staleDogs.length;

  for (const dog of staleDogs) {
    const daysSinceSync = (now.getTime() - new Date(dog.last_synced_at).getTime()) / (1000 * 60 * 60 * 24);

    // More than 90 days without sync — auto-deactivate
    if (daysSinceSync > 90) {
      logger.log({
        audit_type: "stale_listing",
        entity_type: "dog",
        entity_id: dog.id,
        severity: "warning",
        message: `Not synced in ${Math.round(daysSinceSync)} days — marking unavailable`,
        details: { name: dog.name, last_synced_at: dog.last_synced_at },
        action_taken: "Set is_available=false, status=transferred",
      });

      await supabase
        .from("dogs")
        .update({
          is_available: false,
          status: "transferred",
          last_audited_at: now.toISOString(),
        })
        .eq("id", dog.id);
      deactivated++;
    } else {
      // 30-90 days — warn but don't deactivate yet
      logger.log({
        audit_type: "stale_listing",
        entity_type: "dog",
        entity_id: dog.id,
        severity: "info",
        message: `Not synced in ${Math.round(daysSinceSync)} days — monitor`,
        details: { name: dog.name, last_synced_at: dog.last_synced_at },
      });
    }
    stale++;
  }

  return { checked, stale, deactivated };
}

// ============================================================
// 3. DUPLICATE DETECTION — same external_id or name+shelter combos
// ============================================================
export async function checkDuplicates(logger: AuditLogger): Promise<{
  checked: number;
  duplicates: number;
  removed: number;
}> {
  const supabase = createAdminClient();
  let duplicates = 0;
  let removed = 0;

  // Check for duplicate external_ids (should be unique per source)
  const { data: dogs } = await supabase
    .from("dogs")
    .select("external_id, id, name, created_at")
    .eq("external_source", "rescuegroups")
    .not("external_id", "is", null)
    .order("external_id")
    .order("created_at", { ascending: true });

  if (!dogs) return { checked: 0, duplicates: 0, removed: 0 };

  const seen = new Map<string, { id: string; name: string; created_at: string }>();

  for (const dog of dogs) {
    if (!dog.external_id) continue;

    if (seen.has(dog.external_id)) {
      const original = seen.get(dog.external_id)!;
      duplicates++;

      logger.log({
        audit_type: "duplicate_check",
        entity_type: "dog",
        entity_id: dog.id,
        severity: "warning",
        message: `Duplicate external_id ${dog.external_id} — keeping older record`,
        details: {
          duplicate_name: dog.name,
          original_id: original.id,
          original_name: original.name,
        },
        action_taken: "Deleted duplicate record",
      });

      // Delete the newer duplicate
      await supabase.from("dogs").delete().eq("id", dog.id);
      removed++;
    } else {
      seen.set(dog.external_id, {
        id: dog.id,
        name: dog.name,
        created_at: dog.created_at,
      });
    }
  }

  return { checked: dogs.length, duplicates, removed };
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
  let checked = 0;
  let issues = 0;
  let repaired = 0;
  const now = new Date();

  // Dogs with expired euthanasia dates still marked available
  const { data: expiredDogs } = await supabase
    .from("dogs")
    .select("id, name, euthanasia_date, urgency_level, status")
    .eq("is_available", true)
    .not("euthanasia_date", "is", null)
    .lt("euthanasia_date", now.toISOString());

  if (expiredDogs) {
    checked += expiredDogs.length;
    for (const dog of expiredDogs) {
      const hoursExpired =
        (now.getTime() - new Date(dog.euthanasia_date!).getTime()) / (1000 * 60 * 60);

      // If expired more than 7 days ago, mark critical
      if (hoursExpired > 168) {
        logger.log({
          audit_type: "status_check",
          entity_type: "dog",
          entity_id: dog.id,
          severity: "critical",
          message: `Euthanasia date expired ${Math.round(hoursExpired / 24)} days ago — still listed as available`,
          details: {
            name: dog.name,
            euthanasia_date: dog.euthanasia_date,
            hours_expired: Math.round(hoursExpired),
          },
          action_taken: "Set urgency_level=critical, flagged for review",
        });

        await supabase
          .from("dogs")
          .update({
            urgency_level: "critical",
            last_audited_at: now.toISOString(),
          })
          .eq("id", dog.id);
        repaired++;
      }
      issues++;
    }
  }

  // Dogs with status not matching is_available flag
  const { data: mismatchDogs } = await supabase
    .from("dogs")
    .select("id, name, status, is_available")
    .eq("is_available", true)
    .in("status", ["adopted", "deceased", "euthanized", "returned_to_owner"]);

  if (mismatchDogs) {
    checked += mismatchDogs.length;
    for (const dog of mismatchDogs) {
      logger.log({
        audit_type: "status_check",
        entity_type: "dog",
        entity_id: dog.id,
        severity: "error",
        message: `Status "${dog.status}" but is_available=true — deactivating`,
        details: { name: dog.name, status: dog.status },
        action_taken: "Set is_available=false",
      });

      await supabase
        .from("dogs")
        .update({ is_available: false, last_audited_at: now.toISOString() })
        .eq("id", dog.id);
      repaired++;
      issues++;
    }
  }

  return { checked, issues, repaired };
}

// ============================================================
// 5. PHOTO VALIDATION — check for missing/empty photo data
// ============================================================
export async function checkPhotos(logger: AuditLogger): Promise<{
  checked: number;
  missing: number;
}> {
  const supabase = createAdminClient();

  // Count available dogs without photos
  const { count, data } = await supabase
    .from("dogs")
    .select("id, name, breed_primary", { count: "exact" })
    .eq("is_available", true)
    .is("primary_photo_url", null)
    .limit(50);

  const missing = count ?? 0;

  if (missing > 0) {
    logger.log({
      audit_type: "photo_check",
      entity_type: "dog",
      severity: "info",
      message: `${missing} available dogs without photos`,
      details: {
        total_missing: missing,
        sample: (data || []).slice(0, 10).map((d) => ({
          id: d.id,
          name: d.name,
          breed: d.breed_primary,
        })),
      },
    });
  }

  // Check for dogs with empty photo_urls arrays but a primary_photo_url
  const { count: inconsistentCount } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .eq("is_available", true)
    .not("primary_photo_url", "is", null)
    .eq("photo_urls", "{}");

  if (inconsistentCount && inconsistentCount > 0) {
    logger.log({
      audit_type: "photo_check",
      entity_type: "dog",
      severity: "warning",
      message: `${inconsistentCount} dogs have primary_photo_url but empty photo_urls array`,
    });
  }

  return { checked: missing + (inconsistentCount ?? 0), missing };
}

// ============================================================
// 6. SHELTER VALIDATION — orphaned or incomplete shelters
// ============================================================
export async function checkShelters(logger: AuditLogger): Promise<{
  checked: number;
  issues: number;
}> {
  const supabase = createAdminClient();
  let issues = 0;

  // Shelters with no dogs
  const { data: shelters } = await supabase
    .from("shelters")
    .select("id, name, state_code, phone, email, website")
    .eq("is_active", true);

  if (!shelters) return { checked: 0, issues: 0 };

  // Get shelters that have dogs
  const { data: sheltersWithDogs } = await supabase
    .from("dogs")
    .select("shelter_id")
    .eq("is_available", true);

  const activeSheltIds = new Set(
    (sheltersWithDogs || []).map((d) => d.shelter_id)
  );

  // Shelters missing critical contact info
  let missingContact = 0;
  for (const shelter of shelters) {
    if (!shelter.phone && !shelter.email && !shelter.website) {
      missingContact++;
    }
  }

  if (missingContact > 0) {
    logger.log({
      audit_type: "shelter_check",
      entity_type: "shelter",
      severity: "info",
      message: `${missingContact} active shelters have no phone, email, or website`,
    });
    issues += missingContact;
  }

  const verifiedWithNoDogs = shelters.filter(
    (s) => !activeSheltIds.has(s.id)
  ).length;

  if (verifiedWithNoDogs > 0) {
    logger.log({
      audit_type: "shelter_check",
      entity_type: "shelter",
      severity: "info",
      message: `${verifiedWithNoDogs} active shelters have no available dogs listed`,
    });
  }

  return { checked: shelters.length, issues };
}

// ============================================================
// 7. DATA QUALITY — names, descriptions, breed data
// ============================================================
export async function checkDataQuality(logger: AuditLogger): Promise<{
  checked: number;
  issues: number;
  repaired: number;
}> {
  const supabase = createAdminClient();
  let checked = 0;
  let issues = 0;
  let repaired = 0;
  let offset = 0;
  const now = new Date();

  while (true) {
    const { data: dogs } = await supabase
      .from("dogs")
      .select("id, name, breed_primary, description")
      .eq("is_available", true)
      .range(offset, offset + BATCH_SIZE - 1)
      .order("id");

    if (!dogs || dogs.length === 0) break;
    checked += dogs.length;

    for (const dog of dogs) {
      // Generic placeholder names
      const genericNames = [
        "Unknown",
        "N/A",
        "No Name",
        "TBD",
        "Unnamed",
        "Dog",
        "Test",
      ];
      if (genericNames.includes(dog.name)) {
        logger.log({
          audit_type: "data_quality",
          entity_type: "dog",
          entity_id: dog.id,
          severity: "warning",
          message: `Generic/placeholder name: "${dog.name}"`,
          details: { breed: dog.breed_primary },
        });
        issues++;
      }

      // Names that are just IDs (e.g., "A030521")
      if (/^[A-Z]\d{5,}$/.test(dog.name)) {
        logger.log({
          audit_type: "data_quality",
          entity_type: "dog",
          entity_id: dog.id,
          severity: "info",
          message: `Name appears to be a shelter ID code: "${dog.name}"`,
          details: { breed: dog.breed_primary },
        });
        issues++;
      }

      // Descriptions with HTML or encoding artifacts
      if (
        dog.description &&
        (dog.description.includes("<") ||
          dog.description.includes("&lt;") ||
          dog.description.includes("&#"))
      ) {
        logger.log({
          audit_type: "data_quality",
          entity_type: "dog",
          entity_id: dog.id,
          severity: "warning",
          message: "Description contains HTML/encoding artifacts",
          details: { sample: dog.description.substring(0, 200) },
          action_taken: "Cleaned HTML entities",
        });

        const cleaned = dog.description
          .replace(/<[^>]+>/g, "")
          .replace(/&lt;/g, "<")
          .replace(/&gt;/g, ">")
          .replace(/&amp;/g, "&")
          .replace(/&nbsp;/g, " ")
          .replace(/&#\d+;/g, "")
          .replace(/\s+/g, " ")
          .trim();

        await supabase
          .from("dogs")
          .update({ description: cleaned, last_audited_at: now.toISOString() })
          .eq("id", dog.id);
        repaired++;
        issues++;
      }
    }

    offset += dogs.length;
    if (offset % 5000 === 0) await logger.flush();
  }

  return { checked, issues, repaired };
}
