import { createAdminClient } from "@/lib/supabase/admin";

interface FoundContact {
  email: string;
  name: string | null;
  source: "database" | "website_scrape" | "rg_api";
}

/**
 * Find contact email for a shelter.
 * Checks multiple sources in priority order.
 */
export async function findShelterContact(
  shelterId: string
): Promise<FoundContact | null> {
  const supabase = createAdminClient();

  // 1. Check shelter_contacts table (registered contacts)
  const { data: contacts } = await supabase
    .from("shelter_contacts")
    .select("email, first_name, last_name")
    .eq("shelter_id", shelterId)
    .eq("is_active", true)
    .eq("is_opted_out", false)
    .order("is_primary", { ascending: false })
    .limit(1);

  if (contacts && contacts.length > 0) {
    return {
      email: contacts[0].email,
      name: `${contacts[0].first_name} ${contacts[0].last_name}`.trim() || null,
      source: "database",
    };
  }

  // 2. Check shelter's email field
  const { data: shelter } = await supabase
    .from("shelters")
    .select("email, name")
    .eq("id", shelterId)
    .single();

  if (shelter?.email) {
    return {
      email: shelter.email,
      name: null,
      source: "database",
    };
  }

  // 3. No contact found
  return null;
}

/**
 * Find contacts for shelters matching criteria.
 * Used for building outreach target lists.
 */
export async function findTargetShelters(filters: {
  state?: string;
  min_dogs?: number;
  has_email?: boolean;
  not_contacted?: boolean;
  limit?: number;
}): Promise<
  Array<{
    shelter_id: string;
    shelter_name: string;
    email: string;
    city: string;
    state: string;
    dog_count: number;
  }>
> {
  const supabase = createAdminClient();

  let query = supabase
    .from("shelters")
    .select("id, name, email, city, state, total_dogs");

  if (filters.state) {
    query = query.eq("state", filters.state);
  }

  if (filters.has_email) {
    query = query.not("email", "is", null);
  }

  if (filters.min_dogs) {
    query = query.gte("total_dogs", filters.min_dogs);
  }

  // Only shelters not already partners
  query = query.or("partner_status.is.null,partner_status.eq.none");

  query = query
    .order("total_dogs", { ascending: false })
    .limit(filters.limit || 100);

  const { data } = await query;
  if (!data) return [];

  // If filtering for not-contacted, exclude already-contacted shelters
  if (filters.not_contacted) {
    const shelterIds = data.map((s) => s.id);
    const { data: contacted } = await supabase
      .from("outreach_targets")
      .select("shelter_id")
      .in("shelter_id", shelterIds);

    const contactedSet = new Set((contacted || []).map((c) => c.shelter_id));
    return data
      .filter((s) => !contactedSet.has(s.id))
      .map((s) => ({
        shelter_id: s.id,
        shelter_name: s.name,
        email: s.email || "",
        city: s.city || "",
        state: s.state || "",
        dog_count: s.total_dogs || 0,
      }));
  }

  return data.map((s) => ({
    shelter_id: s.id,
    shelter_name: s.name,
    email: s.email || "",
    city: s.city || "",
    state: s.state || "",
    dog_count: s.total_dogs || 0,
  }));
}

/**
 * Count shelters available for outreach.
 */
export async function countAvailableTargets(): Promise<{
  total: number;
  with_email: number;
  not_contacted: number;
}> {
  const supabase = createAdminClient();

  const [totalRes, emailRes] = await Promise.all([
    supabase
      .from("shelters")
      .select("id", { count: "exact", head: true })
      .or("partner_status.is.null,partner_status.eq.none"),
    supabase
      .from("shelters")
      .select("id", { count: "exact", head: true })
      .not("email", "is", null)
      .or("partner_status.is.null,partner_status.eq.none"),
  ]);

  const { count: contactedCount } = await supabase
    .from("outreach_targets")
    .select("id", { count: "exact", head: true });

  return {
    total: totalRes.count || 0,
    with_email: emailRes.count || 0,
    not_contacted: (emailRes.count || 0) - (contactedCount || 0),
  };
}
