import { createAdminClient } from "@/lib/supabase/admin";
import type {
  ShelterCommunication,
  LogCommunicationInput,
} from "@/types/shelter-crm";

const supabase = createAdminClient();

export async function logCommunication(
  input: LogCommunicationInput
): Promise<ShelterCommunication> {
  const { data, error } = await supabase
    .from("shelter_communications")
    .insert({
      shelter_id: input.shelter_id,
      contact_id: input.contact_id || null,
      comm_type: input.comm_type,
      direction: input.direction || "outbound",
      subject: input.subject || null,
      body: input.body || null,
      body_html: input.body_html || null,
      status: input.status || "sent",
      priority: input.priority || "normal",
      sent_by: input.sent_by || "system",
      channel: input.channel || null,
      template_id: input.template_id || null,
      external_message_id: input.external_message_id || null,
      related_entity_type: input.related_entity_type || null,
      related_entity_id: input.related_entity_id || null,
      metadata: input.metadata || {},
      scheduled_for: input.scheduled_for || null,
    })
    .select()
    .single();

  if (error)
    throw new Error(`Failed to log communication: ${error.message}`);

  // Increment shelter total_communications
  const { data: shelter } = await supabase
    .from("shelters")
    .select("total_communications")
    .eq("id", input.shelter_id)
    .single();
  if (shelter) {
    await supabase
      .from("shelters")
      .update({
        total_communications: (shelter.total_communications || 0) + 1,
      })
      .eq("id", input.shelter_id);
  }

  // Increment contact total_communications if applicable
  if (input.contact_id) {
    const { data: contact } = await supabase
      .from("shelter_contacts")
      .select("total_communications")
      .eq("id", input.contact_id)
      .single();
    if (contact) {
      await supabase
        .from("shelter_contacts")
        .update({
          total_communications: (contact.total_communications || 0) + 1,
          last_contacted_at:
            input.direction !== "inbound"
              ? new Date().toISOString()
              : undefined,
          last_responded_at:
            input.direction === "inbound"
              ? new Date().toISOString()
              : undefined,
        })
        .eq("id", input.contact_id);
    }
  }

  return data as ShelterCommunication;
}

export async function getCommunicationHistory(
  shelterId: string,
  options: {
    limit?: number;
    offset?: number;
    comm_type?: string;
    direction?: string;
  } = {}
): Promise<{ communications: ShelterCommunication[]; total: number }> {
  const { limit = 50, offset = 0, comm_type, direction } = options;

  let query = supabase
    .from("shelter_communications")
    .select("*", { count: "exact" })
    .eq("shelter_id", shelterId)
    .order("created_at", { ascending: false })
    .range(offset, offset + limit - 1);

  if (comm_type) query = query.eq("comm_type", comm_type);
  if (direction) query = query.eq("direction", direction);

  const { data, error, count } = await query;
  if (error)
    throw new Error(`Failed to get communications: ${error.message}`);

  return {
    communications: (data || []) as ShelterCommunication[],
    total: count || 0,
  };
}

export async function getCommunicationById(
  id: string
): Promise<ShelterCommunication | null> {
  const { data, error } = await supabase
    .from("shelter_communications")
    .select("*")
    .eq("id", id)
    .single();

  if (error) return null;
  return data as ShelterCommunication;
}

export async function updateCommunicationStatus(
  id: string,
  status: string,
  metadata?: Record<string, unknown>
): Promise<void> {
  const update: Record<string, unknown> = { status };

  if (status === "opened") update.opened_at = new Date().toISOString();
  if (status === "clicked") update.clicked_at = new Date().toISOString();
  if (status === "replied") update.replied_at = new Date().toISOString();
  if (metadata) update.metadata = metadata;

  await supabase
    .from("shelter_communications")
    .update(update)
    .eq("id", id);
}

export async function getQueuedCommunications(
  limit = 50
): Promise<ShelterCommunication[]> {
  const { data, error } = await supabase
    .from("shelter_communications")
    .select("*")
    .in("status", ["queued", "draft"])
    .not("scheduled_for", "is", null)
    .lte("scheduled_for", new Date().toISOString())
    .order("priority", { ascending: true })
    .order("scheduled_for", { ascending: true })
    .limit(limit);

  if (error)
    throw new Error(`Failed to get queued communications: ${error.message}`);
  return (data || []) as ShelterCommunication[];
}

export async function getUnreadCount(shelterId: string): Promise<number> {
  const { count, error } = await supabase
    .from("shelter_communications")
    .select("*", { count: "exact", head: true })
    .eq("shelter_id", shelterId)
    .eq("direction", "inbound")
    .eq("status", "received");

  if (error) return 0;
  return count || 0;
}

export async function markRead(communicationId: string): Promise<void> {
  await supabase
    .from("shelter_communications")
    .update({ status: "read" })
    .eq("id", communicationId)
    .eq("status", "received");
}
