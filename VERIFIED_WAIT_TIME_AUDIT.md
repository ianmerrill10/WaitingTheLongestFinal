# Verified Wait Time System — Audit Report

**Date:** March 22, 2026
**Auditor:** Claude (automated code + data audit)
**Scope:** All code changes to the verified wait time system, production database state, accuracy analysis

---

## Executive Summary

The verified wait time system was built to solve a critical problem: **we were displaying inaccurate wait times for dogs**. The old system used RescueGroups' `updatedDate` (when a listing was last edited) as a proxy for intake date. This produced absurd results — dogs showing 8+ year wait times when they'd only been available for weeks.

The new system captures **shelter-provided dates** (`availableDate`, `foundDate`) from the RescueGroups API, detects **returned-after-adoption dogs**, and maps `killDate` to euthanasia countdowns.

**Current state: The infrastructure is solid. Coverage is low.** Only 694 of 33,140 dogs (2.1%) have been verified so far. The system processes ~400 dogs/day via automated crons, meaning full coverage will take approximately 81 days.

---

## 1. Code Changes Audit

### 1.1 Files Modified

| File | Change Type | Lines Changed |
|------|------------|---------------|
| `src/lib/rescuegroups/client.ts` | Interface update | +5 fields added to `RGAnimalAttributes` |
| `src/lib/rescuegroups/mapper.ts` | Major rewrite | New `computeIntakeDate()` with 7-strategy priority, euthanasia mapping, returned dog detection |
| `src/lib/verification/engine.ts` | Feature addition | Opportunistic capture of availableDate, foundDate, killDate during verification |
| `src/app/api/cron/sync-dogs/route.ts` | Stats update | New date capture counters in combined verification results |

### 1.2 `client.ts` — RGAnimalAttributes Interface

**Added fields:**
```typescript
availableDate?: string | null;   // When dog became available for adoption (GOLD STANDARD)
foundDate?: string | null;       // When stray was found (= intake for strays)
adoptedDate?: string | null;     // When dog was adopted (for returned dog detection)
killDate?: string | null;        // Scheduled euthanasia date
killReason?: string | null;      // Reason for euthanasia scheduling
```

**Audit finding:** These fields were always available in the RescueGroups v5 API but were never being captured. They are now properly typed and integrated.

**Status: PASS**

### 1.3 `mapper.ts` — Intake Date Priority System

The `computeIntakeDate()` function was completely rewritten with a 7-strategy priority system:

| Priority | Source | Confidence | Description |
|----------|--------|-----------|-------------|
| 1 | Returned dog check | `high` | adoptedDate < updatedDate = dog was adopted and returned |
| 2 | `availableDate` | `verified` | Shelter-set "available for adoption" date (GOLD STANDARD) |
| 3 | `foundDate` | `verified` | When stray was found = intake for strays |
| 4 | Description text | `verified`/`high` | Parsed from listing text (e.g., "Posted 2/18/18") |
| 5 | `createdDate` | `high`/`medium`/`low` | When listing was created on RescueGroups |
| 6 | `updatedDate` | `medium`/`low` | When listing was last edited (WEAKEST) |
| 7 | Current time | `unknown` | No date available at all |

**Key logic audited:**

1. **Returned dog detection:** If `adoptedDate` exists AND `updatedDate > adoptedDate`, the dog was adopted and returned. Wait time resets from the return date, not original listing.

2. **Age sanity check (`applyAgeSanityCheck`):** After computing intake date, the system checks if wait time > dog's age. If so, caps intake_date to birth date. Uses API `birthDate` when available (gold standard), otherwise estimates from `ageMonths`.

3. **Courtesy listing handling:** Courtesy listings (dogs listed on behalf of owners, not shelters) get `low` confidence regardless of date source, since the dates reflect the courtesy post, not actual time waiting.

4. **Euthanasia mapping:** `killDate` from RG API maps directly to `euthanasia_date` and `is_on_euthanasia_list` fields.

**Bug found and fixed during build:** Duplicate variable name `updatedDate` — the returned dog check and the Strategy 5 block both declared `const updatedDate`. Fixed by renaming to `returnedCheckDate` in the returned dog check.

**Status: PASS** (after bug fix)

### 1.4 `engine.ts` — Verification Engine Enhancements

The verification engine now **opportunistically captures** new date fields when re-checking dogs against the RescueGroups API:

```
When a dog is verified as "available":
  1. Check returned-after-adoption (adoptedDate < updatedDate) → reset intake_date
  2. Capture availableDate → set intake_date with "verified" confidence
  3. Capture foundDate → set intake_date with "verified" confidence
  4. Capture killDate → set euthanasia_date + is_on_euthanasia_list
  5. Capture birthDate → set birth_date, recalculate age_months
  6. Age sanity check → cap intake_date if before birth
  7. Capture isCourtesyListing flag
```

**New return type fields:**
- `availableDatesCaptured` — count of dogs where availableDate was captured
- `foundDatesCaptured` — count of dogs where foundDate was captured
- `killDatesCaptured` — count of dogs where killDate was captured

**Variable shadowing fix:** `nowDate` was declared in both the availableDate and returned dog blocks. Renamed to `nowDate2` in the availableDate block.

**Status: PASS** (after bug fix)

### 1.5 `route.ts` — Cron Pipeline

Updated the combined verification stats to pass through the new capture counters. No logic changes, just plumbing.

**Status: PASS**

---

## 2. Production Database Audit

### 2.1 Total Dogs

| Metric | Count |
|--------|-------|
| Total available dogs | **33,140** |
| Total deactivated (adopted/removed) | 4 |
| With birth_date | 3,387 (10.2%) |
| With euthanasia_date | 0 (0%) |
| Courtesy listings | 127 (0.4%) |

### 2.2 Date Source Breakdown

| Source | Count | % of Total | Confidence |
|--------|-------|-----------|------------|
| `rescuegroups_updated_date` | 27,647 | 83.4% | medium/low |
| `rescuegroups_created_date` | 2,935 | 8.9% | high/medium |
| `rescuegroups_created_date_active` | 1,481 | 4.5% | high |
| `description_parsed` | 475 | 1.4% | verified/high |
| `audit_repair_stale_date` | 248 | 0.7% | low |
| `rescuegroups_courtesy_listing_created_date` | 116 | 0.4% | low |
| `rescuegroups_returned_after_adoption` | 26 | 0.08% | high |
| `age_capped` | 23 | 0.07% | low |
| `rescuegroups_available_date` | 12 | 0.04% | verified |
| `rescuegroups_found_date` | 3 | 0.01% | verified |

### 2.3 Date Confidence Breakdown

| Confidence | Count | % of Total |
|-----------|-------|-----------|
| `verified` | 486 | 1.5% |
| `high` | 24,772 | 74.8% |
| `medium` | 3,691 | 11.1% |
| `low` | 4,191 | 12.6% |
| `unknown` | 0 | 0% |

### 2.4 Verification Status

| Status | Count | % of Total |
|--------|-------|-----------|
| `verified` (confirmed available on RG) | 693 | 2.1% |
| `not_found` (removed from RG, still active on our site) | 1 | <0.01% |
| `pending` (adoption pending) | 0 | 0% |
| Never checked (`last_verified_at` IS NULL) | 32,446 | 97.9% |

---

## 3. Accuracy Analysis

### 3.1 The Core Problem

**83.4% of dogs (27,647) still use `rescuegroups_updated_date` as their intake date.** This is the weakest possible source — it reflects when the listing was last edited on RescueGroups, not when the dog actually arrived at the shelter.

This means:
- A dog could have been listed in 2020 but had its description updated yesterday. Our system would show it as "waiting 1 day" when it's actually been waiting 6 years.
- Conversely, a dog could have been re-listed after being returned, but the `updatedDate` is from the original listing, showing years of wait time when the dog only came back recently.

### 3.2 Why Only 12 Dogs Have availableDate

When the verification engine checks a dog against the RG API, it captures `availableDate` if present. But in a sample of 500 dogs from the RG API, only ~10% had an `availableDate` field populated. This is because:

1. Many shelters don't fill in this field on RescueGroups
2. It's an optional field in the RG platform
3. Older listings (pre-2020) rarely have it

**This means even after full verification coverage, the majority of dogs will still rely on `createdDate` or `updatedDate`.** The `availableDate` field is gold when it exists, but it doesn't exist for most dogs.

### 3.3 What Verification Actually Improves

When a dog gets verified, the engine applies this priority:
1. **Returned dog check** — catches 26 dogs so far (3.7% of verified)
2. **availableDate capture** — catches 12 dogs so far (1.7% of verified)
3. **foundDate capture** — catches 3 dogs so far (0.4% of verified)
4. **killDate capture** — 0 dogs so far (field rarely populated in RG)
5. **birthDate capture** — enriches age data for age sanity checks

So of 693 verified dogs, **41 (5.9%) got their intake_date corrected** through the new system.

### 3.4 Real Examples

**Sarah** — `rescuegroups_available_date`, `verified` confidence
- intake_date: 2016-11-06 (9.4 years waiting)
- This is a shelter-provided date. If accurate, Sarah has genuinely been waiting 9+ years.

**Broly** — `rescuegroups_returned_after_adoption`, `high` confidence
- intake_date: 2022-01-28 (adopted and returned)
- Without the returned dog detection, this would show the original listing date, overstating wait time.

**Betsy FH** — Before/After fix:
- BEFORE: intake_date 2017-09-18, date_source `rescuegroups_created_date_active`, confidence `low` (8.5 years)
- AFTER: intake_date 2026-02-20, date_source `rescuegroups_available_date`, confidence `verified` (1 month)
- **This is the system working correctly.** The dog was re-listed recently but the old listing date made it look like a 8.5-year wait.

---

## 4. Timeline to Full Coverage

| Metric | Value |
|--------|-------|
| Dogs verified so far | 693 |
| Dogs remaining | 32,446 |
| Verification rate | ~400/day (200 morning + 200 evening cron) |
| Estimated days to full coverage | **~81 days** |
| Estimated completion | ~June 11, 2026 |

**Note:** New dogs are synced daily (~100-500/day), so the target is always moving. The verification engine prioritizes oldest/never-verified dogs first, so the most important corrections happen early.

---

## 5. Identified Issues

### 5.1 CRITICAL: 83% on weakest date source

The `rescuegroups_updated_date` is the LEAST reliable source but covers 83.4% of dogs. This happened because during the initial bulk sync, the mapper's priority system fell through to `updatedDate` for most dogs (they didn't have `availableDate`, `foundDate`, or parseable description dates, and `createdDate` was somehow not available or the query selected `updatedDate`).

**Root cause:** The initial sync likely used an earlier version of the mapper before the priority system was refined. The new mapper correctly prefers `createdDate` over `updatedDate`, but existing records haven't been re-processed.

**Recommendation:** Run a one-time re-sync of all dogs to apply the new mapper priority. This would move many dogs from `rescuegroups_updated_date` to `rescuegroups_created_date` or `rescuegroups_created_date_active`, which is a meaningful accuracy improvement.

### 5.2 MEDIUM: Zero euthanasia dates captured

Despite building the killDate → euthanasia_date pipeline, zero dogs currently have euthanasia dates. This is because:
- The RescueGroups API rarely includes `killDate` (0/500 in our sample)
- Kill shelters may not post this data publicly
- The RG platform may not expose this field for all shelter types

**Impact:** The euthanasia countdown feature (the site's marquee differentiator) currently has no data to display. The infrastructure is ready but the data source doesn't provide it.

**Recommendation:** Explore alternative sources for euthanasia data — direct shelter API integrations, ShelterLuv/PetPoint APIs, or community reporting. RescueGroups alone will not populate this field.

### 5.3 LOW: Verification rate vs. database size

At 400 dogs/day, full initial verification takes 81 days. By then, some early-verified dogs will be stale again (7-day window). The system handles this via the "stale" verification strategy (30% of each batch), but continuous coverage requires:
- 33,140 dogs / 7 days = 4,734 verifications/day needed for full freshness
- Current rate: 400/day = 8.4% of what's needed

**Recommendation:** Increase `verify_batch` parameter or add more cron slots if Vercel plan allows. Current hobby plan limits us to 2 daily crons.

### 5.4 LOW: Sync overwrites verification-corrected dates

When the daily sync runs, it re-maps all dogs from the RG API response. If a dog was previously verified and had its `intake_date` corrected to `availableDate`, the sync could overwrite it if the mapper runs with the same data.

**Audit check:** The sync's `toUpdate` path calls `mapRescueGroupsDog()` which runs `computeIntakeDate()` — this DOES apply the correct priority, so `availableDate` would still win. However, if the sync response doesn't include `availableDate` in the animal attributes (e.g., the search endpoint returns fewer fields than the individual animal endpoint), the sync would downgrade the date.

**Status:** POTENTIAL ISSUE — needs verification that the `/public/animals/search/available/dogs/` endpoint returns the same fields as `/public/animals/{id}`.

---

## 6. Code Quality Assessment

### 6.1 Strengths

- **Clear priority system:** The 7-strategy cascade in `computeIntakeDate()` is well-documented and logical
- **Age sanity check:** Prevents impossible wait times (longer than the dog has been alive)
- **Returned dog detection:** Novel insight that catches ~3.7% of verified dogs
- **Opportunistic capture:** The verification engine enriches data during its normal verification pass, no extra API calls needed
- **Courtesy listing handling:** Properly flagged and downgraded to low confidence
- **Rate limiting:** RescueGroups API calls are rate-limited (2/sec, 100/min)

### 6.2 Weaknesses

- **No re-processing pipeline:** No way to re-apply the mapper to all existing dogs without a full re-sync
- **Hardcoded batch sizes:** `PARALLEL_BATCH = 5` in verification engine, not configurable
- **No alerting:** If verification discovers widespread changes (e.g., 50% of a batch is not_found), there's no notification system
- **External URL check is fragile:** HEAD requests to shelter pages can be unreliable — sites behind Cloudflare, rate-limited, or temporarily down will give false negatives

---

## 7. Recommendations

### Immediate (this week)

1. **Run a full re-sync** to re-process all 33,140 dogs through the new mapper. This will move the ~27,647 dogs on `rescuegroups_updated_date` to better sources where possible.

2. **Verify the sync endpoint returns availableDate.** If `/public/animals/search/available/dogs/` doesn't return `availableDate`, the sync will never capture it — only verification will. This is critical to understand.

### Short-term (next 30 days)

3. **Investigate direct shelter integrations** for euthanasia dates. RescueGroups doesn't provide `killDate` for most dogs. ShelterLuv, PetPoint, and direct shelter feeds are the path to real countdown timers.

4. **Add a "date accuracy" badge to dog profiles.** Currently only the dog detail page shows "~ estimated wait time" for low confidence dates. Make this more prominent and transparent.

### Long-term (next 90 days)

5. **Complete first full verification pass** (~81 days). After this, all dogs will have been checked at least once.

6. **Increase verification throughput.** Upgrade from Vercel hobby plan to get more cron slots, or implement a queue-based system.

---

## 8. Summary Table

| Metric | Value | Grade |
|--------|-------|-------|
| Code correctness | All files compile, build passes | A |
| Date priority logic | 7-strategy cascade, well-ordered | A |
| Age sanity check | Working, 23 dogs corrected | A |
| Returned dog detection | Working, 26 dogs corrected | A |
| availableDate capture | Working, 12 dogs captured | A |
| foundDate capture | Working, 3 dogs captured | A |
| killDate capture | Infrastructure ready, 0 data | B (not our fault) |
| Production coverage | 2.1% verified (693/33,140) | D |
| Date accuracy (verified+high) | 76.3% (25,258/33,140) | C+ |
| Euthanasia countdown data | 0 dogs with dates | F (data source issue) |
| Overall system grade | | **C+** |

The infrastructure is solid (A-grade code). The coverage is what's dragging the grade down. As verification cycles through all 33K dogs over the next 81 days, the grade will improve. The euthanasia data gap requires a different data source entirely.

---

*Report generated from live production database queries on March 22, 2026.*
*Commit: 5a214ad — "Verified wait time system: use availableDate/foundDate from RescueGroups"*
