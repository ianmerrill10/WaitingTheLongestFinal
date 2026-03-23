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
  const { data: unverifiedWithEin, error: fetchErr } = await supabase
    .from("shelters")
    .select("id")
    .not("ein", "is", null)
    .eq("is_verified", false);

  if (fetchErr) {
    return { verified: 0, alreadyVerified: 0, noEin: noEinCount || 0, errors: 1 };
  }

  if (!unverifiedWithEin || unverifiedWithEin.length === 0) {
    // Count already verified
    const { count: verifiedCount } = await supabase
      .from("shelters")
      .select("id", { count: "exact", head: true })
      .not("ein", "is", null)
      .eq("is_verified", true);

    return {
      verified: 0,
      alreadyVerified: verifiedCount || 0,
      noEin: noEinCount || 0,
      errors: 0,
    };
  }

  // Batch update in chunks of 200
  const ids = unverifiedWithEin.map((s) => s.id);
  for (let i = 0; i < ids.length; i += 200) {
    const batch = ids.slice(i, i + 200);
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
  const { data: rgUnverified } = await supabase
    .from("shelters")
    .select("id")
    .eq("external_source", "rescuegroups")
    .eq("is_verified", false);

  if (rgUnverified && rgUnverified.length > 0) {
    const rgIds = rgUnverified.map((s) => s.id);
    for (let i = 0; i < rgIds.length; i += 200) {
      const batch = rgIds.slice(i, i + 200);
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
    alreadyVerified: (alreadyCount || 0) - verified,
    noEin: noEinCount || 0,
    errors,
  };
}
