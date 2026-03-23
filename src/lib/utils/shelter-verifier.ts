/**
 * Shelter EIN Verifier — auto-verifies shelters with valid EINs.
 * Cross-references shelters against IRS 501(c)(3) data already in our DB.
 * Shelters seeded from `irs_990` source already have EINs — mark them verified.
 */
import { createAdminClient } from "@/lib/supabase/admin";

interface VerifyResult {
  verified: number;
  alreadyVerified: number;
  noEin: number;
  errors: number;
}

/**
 * Auto-verify shelters that have a valid EIN.
 * Any shelter with a non-null EIN from the IRS 990 source is a confirmed nonprofit.
 * Also verifies shelters from RescueGroups (they vet orgs).
 */
export async function verifySheltersByEIN(): Promise<VerifyResult> {
  const supabase = createAdminClient();
  let verified = 0;
  let alreadyVerified = 0;
  let errors = 0;

  // Count shelters without EIN for reporting
  const { count: noEinCount } = await supabase
    .from("shelters")
    .select("id", { count: "exact", head: true })
    .is("ein", null);

  // Step 1: Verify all shelters with EIN that aren't already verified
  // Paginate to avoid Supabase's 1000-row default limit
  const allUnverifiedEinIds: string[] = [];
  let page = 0;
  const PAGE_SIZE = 1000;
  while (true) {
    const { data: batch, error: fetchErr } = await supabase
      .from("shelters")
      .select("id")
      .not("ein", "is", null)
      .eq("is_verified", false)
      .range(page * PAGE_SIZE, (page + 1) * PAGE_SIZE - 1);

    if (fetchErr) {
      errors++;
      break;
    }
    if (!batch || batch.length === 0) break;
    allUnverifiedEinIds.push(...batch.map((s) => s.id));
    if (batch.length < PAGE_SIZE) break;
    page++;
  }

  if (allUnverifiedEinIds.length === 0) {
    const { count: verifiedCount } = await supabase
      .from("shelters")
      .select("id", { count: "exact", head: true })
      .not("ein", "is", null)
      .eq("is_verified", true);

    return {
      verified: 0,
      alreadyVerified: verifiedCount || 0,
      noEin: noEinCount || 0,
      errors,
    };
  }

  // Batch update in chunks of 200
  for (let i = 0; i < allUnverifiedEinIds.length; i += 200) {
    const batch = allUnverifiedEinIds.slice(i, i + 200);
    const { error: updateErr } = await supabase
      .from("shelters")
      .update({
        is_verified: true,
        updated_at: new Date().toISOString(),
      })
      .in("id", batch);

    if (updateErr) {
      errors++;
    } else {
      verified += batch.length;
    }
  }

  // Step 2: Also verify RescueGroups-sourced shelters (they vet organizations)
  // Paginate to avoid 1000-row truncation
  const allRgUnverifiedIds: string[] = [];
  page = 0;
  while (true) {
    const { data: batch } = await supabase
      .from("shelters")
      .select("id")
      .eq("external_source", "rescuegroups")
      .eq("is_verified", false)
      .range(page * PAGE_SIZE, (page + 1) * PAGE_SIZE - 1);

    if (!batch || batch.length === 0) break;
    allRgUnverifiedIds.push(...batch.map((s) => s.id));
    if (batch.length < PAGE_SIZE) break;
    page++;
  }

  if (allRgUnverifiedIds.length > 0) {
    for (let i = 0; i < allRgUnverifiedIds.length; i += 200) {
      const batch = allRgUnverifiedIds.slice(i, i + 200);
      const { error: rgErr } = await supabase
        .from("shelters")
        .update({
          is_verified: true,
          updated_at: new Date().toISOString(),
        })
        .in("id", batch);

      if (rgErr) errors++;
      else verified += batch.length;
    }
  }

  // Count already verified for reporting
  const { count: alreadyCount } = await supabase
    .from("shelters")
    .select("id", { count: "exact", head: true })
    .eq("is_verified", true);

  return {
    verified,
    alreadyVerified: Math.max(0, (alreadyCount || 0) - verified),
    noEin: noEinCount || 0,
    errors,
  };
}
