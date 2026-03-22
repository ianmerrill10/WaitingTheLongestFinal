import { NextRequest, NextResponse } from "next/server";
import { handleResponse } from "@/lib/outreach/responder";
import { createAdminClient } from "@/lib/supabase/admin";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

/**
 * POST /api/outreach/webhook — SendGrid Inbound Parse webhook
 *
 * Receives incoming email replies from shelter contacts.
 * SendGrid sends parsed email data as multipart form.
 */
export async function POST(req: NextRequest) {
  try {
    // SendGrid sends form data
    const contentType = req.headers.get("content-type") || "";
    let fromEmail = "";
    let subject = "";
    let body = "";

    if (contentType.includes("application/json")) {
      // JSON format (testing/alternative)
      const data = await req.json();
      fromEmail = data.from || data.sender || "";
      subject = data.subject || "";
      body = data.text || data.html || "";
    } else {
      // Form data (SendGrid inbound parse)
      const formData = await req.formData();
      fromEmail = extractEmail(formData.get("from") as string || "");
      subject = (formData.get("subject") as string) || "";
      body = (formData.get("text") as string) || (formData.get("html") as string) || "";
    }

    if (!fromEmail) {
      return NextResponse.json({ error: "No sender email" }, { status: 400 });
    }

    // Find the shelter associated with this email
    const supabase = createAdminClient();
    let shelterId: string | undefined;

    // Check outreach_targets for matching email
    const { data: target } = await supabase
      .from("outreach_targets")
      .select("shelter_id")
      .eq("contact_email", fromEmail)
      .limit(1)
      .maybeSingle();

    if (target) {
      shelterId = target.shelter_id;
    } else {
      // Check shelter_contacts
      const { data: contact } = await supabase
        .from("shelter_contacts")
        .select("shelter_id")
        .eq("email", fromEmail)
        .limit(1)
        .maybeSingle();

      if (contact) {
        shelterId = contact.shelter_id;
      }
    }

    // Handle the response
    const result = await handleResponse({
      from_email: fromEmail,
      subject,
      body: body.slice(0, 10000),
      shelter_id: shelterId,
    });

    return NextResponse.json({
      success: true,
      category: result.category,
      action: result.action_taken,
    });
  } catch (err) {
    console.error("Outreach webhook error:", err);
    return NextResponse.json(
      { error: "Webhook processing failed" },
      { status: 500 }
    );
  }
}

/**
 * Extract email address from a "Name <email>" format string.
 */
function extractEmail(from: string): string {
  const match = from.match(/<([^>]+)>/);
  if (match) return match[1];
  if (from.includes("@")) return from.trim();
  return "";
}
