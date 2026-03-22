import { createAdminClient } from "@/lib/supabase/admin";
import type {
  ShelterContact,
  CreateShelterContactInput,
  UpdateShelterContactInput,
} from "@/types/shelter-crm";

const supabase = createAdminClient();

export async function createContact(
  input: CreateShelterContactInput
): Promise<ShelterContact> {
  // If setting as primary, unset any existing primary for this shelter
  if (input.is_primary) {
    await supabase
      .from("shelter_contacts")
      .update({ is_primary: false })
      .eq("shelter_id", input.shelter_id)
      .eq("is_primary", true);
  }

  const { data, error } = await supabase
    .from("shelter_contacts")
    .insert({
      shelter_id: input.shelter_id,
      first_name: input.first_name,
      last_name: input.last_name,
      title: input.title || null,
      role: input.role || "general",
      email: input.email,
      phone: input.phone || null,
      phone_ext: input.phone_ext || null,
      preferred_contact: input.preferred_contact || "email",
      contact_hours: input.contact_hours || null,
      timezone: input.timezone || null,
      notes: input.notes || null,
      is_primary: input.is_primary || false,
    })
    .select()
    .single();

  if (error) throw new Error(`Failed to create contact: ${error.message}`);

  // If primary, also update shelter's primary_contact_id
  if (input.is_primary && data) {
    await supabase
      .from("shelters")
      .update({ primary_contact_id: data.id })
      .eq("id", input.shelter_id);
  }

  return data as ShelterContact;
}

export async function updateContact(
  contactId: string,
  input: UpdateShelterContactInput
): Promise<ShelterContact> {
  const { data, error } = await supabase
    .from("shelter_contacts")
    .update(input)
    .eq("id", contactId)
    .select()
    .single();

  if (error) throw new Error(`Failed to update contact: ${error.message}`);
  return data as ShelterContact;
}

export async function getContactsByShelter(
  shelterId: string,
  activeOnly = true
): Promise<ShelterContact[]> {
  let query = supabase
    .from("shelter_contacts")
    .select("*")
    .eq("shelter_id", shelterId)
    .order("is_primary", { ascending: false })
    .order("created_at", { ascending: true });

  if (activeOnly) {
    query = query.eq("is_active", true);
  }

  const { data, error } = await query;
  if (error) throw new Error(`Failed to get contacts: ${error.message}`);
  return (data || []) as ShelterContact[];
}

export async function getContactById(
  contactId: string
): Promise<ShelterContact | null> {
  const { data, error } = await supabase
    .from("shelter_contacts")
    .select("*")
    .eq("id", contactId)
    .single();

  if (error) return null;
  return data as ShelterContact;
}

export async function setPrimaryContact(
  shelterId: string,
  contactId: string
): Promise<void> {
  // Unset current primary
  await supabase
    .from("shelter_contacts")
    .update({ is_primary: false })
    .eq("shelter_id", shelterId)
    .eq("is_primary", true);

  // Set new primary
  await supabase
    .from("shelter_contacts")
    .update({ is_primary: true })
    .eq("id", contactId)
    .eq("shelter_id", shelterId);

  // Update shelter reference
  await supabase
    .from("shelters")
    .update({ primary_contact_id: contactId })
    .eq("id", shelterId);
}

export async function deactivateContact(contactId: string): Promise<void> {
  await supabase
    .from("shelter_contacts")
    .update({ is_active: false })
    .eq("id", contactId);
}

export async function optOutContact(contactId: string): Promise<void> {
  await supabase
    .from("shelter_contacts")
    .update({
      is_opted_out: true,
      opted_out_at: new Date().toISOString(),
    })
    .eq("id", contactId);
}

export async function recordContactActivity(
  contactId: string,
  type: "contacted" | "responded"
): Promise<void> {
  const update =
    type === "contacted"
      ? { last_contacted_at: new Date().toISOString() }
      : { last_responded_at: new Date().toISOString() };

  await supabase.from("shelter_contacts").update(update).eq("id", contactId);
}

export async function findContactByEmail(
  email: string
): Promise<ShelterContact | null> {
  const { data } = await supabase
    .from("shelter_contacts")
    .select("*")
    .eq("email", email.toLowerCase())
    .eq("is_active", true)
    .limit(1)
    .single();

  return (data as ShelterContact) || null;
}
