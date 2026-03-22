import { createAdminClient } from "@/lib/supabase/admin";
import { sendEmail } from "./email-sender";
import { renderTemplate, getTemplate } from "./email-templates";

type ResponseCategory =
  | "interested"
  | "questions"
  | "not_interested"
  | "unsubscribe"
  | "bounce"
  | "auto_reply"
  | "unknown";

/**
 * Classify an incoming email response.
 */
export function classifyResponse(
  subject: string,
  body: string
): ResponseCategory {
  const text = `${subject} ${body}`.toLowerCase();

  // Auto-reply detection
  if (
    text.includes("out of office") ||
    text.includes("auto-reply") ||
    text.includes("automatic reply") ||
    text.includes("currently away") ||
    text.includes("on vacation")
  ) {
    return "auto_reply";
  }

  // Bounce detection
  if (
    text.includes("delivery failed") ||
    text.includes("undeliverable") ||
    text.includes("mailbox not found") ||
    text.includes("user unknown") ||
    text.includes("550 5.1.1")
  ) {
    return "bounce";
  }

  // Unsubscribe
  if (
    text.includes("unsubscribe") ||
    text.includes("remove me") ||
    text.includes("stop emailing") ||
    text.includes("opt out") ||
    text.includes("do not contact")
  ) {
    return "unsubscribe";
  }

  // Not interested
  if (
    text.includes("not interested") ||
    text.includes("no thank") ||
    text.includes("no thanks") ||
    text.includes("please stop") ||
    text.includes("don't want") ||
    text.includes("already have")
  ) {
    return "not_interested";
  }

  // Interested signals
  if (
    text.includes("interested") ||
    text.includes("sign up") ||
    text.includes("how do i") ||
    text.includes("tell me more") ||
    text.includes("sounds great") ||
    text.includes("love to") ||
    text.includes("would like to") ||
    text.includes("sign me up") ||
    text.includes("let's do it") ||
    text.includes("yes")
  ) {
    return "interested";
  }

  // Questions
  if (
    text.includes("?") ||
    text.includes("how does") ||
    text.includes("what is") ||
    text.includes("can you") ||
    text.includes("question")
  ) {
    return "questions";
  }

  return "unknown";
}

/**
 * Handle an incoming outreach response.
 */
export async function handleResponse(params: {
  from_email: string;
  subject: string;
  body: string;
  shelter_id?: string;
}): Promise<{
  category: ResponseCategory;
  action_taken: string;
}> {
  const category = classifyResponse(params.subject, params.body);
  const supabase = createAdminClient();

  // Log the communication
  await supabase.from("shelter_communications").insert({
    shelter_id: params.shelter_id || null,
    comm_type: "email",
    direction: "inbound",
    subject: params.subject.slice(0, 200),
    body: params.body.slice(0, 5000),
    from_address: params.from_email,
    status: "received",
  });

  let action_taken = "logged";

  switch (category) {
    case "interested": {
      // Fast-track: send welcome/registration link
      if (params.shelter_id) {
        await supabase
          .from("outreach_targets")
          .update({
            status: "responded",
            response_category: "interested",
            responded_at: new Date().toISOString(),
          })
          .eq("shelter_id", params.shelter_id);
      }

      // Send registration link
      const welcomeTemplate = getTemplate("welcome_partner");
      if (welcomeTemplate) {
        const rendered = renderTemplate(welcomeTemplate, {
          contact_first_name: "there",
          shelter_name: "your shelter",
          register_url: "https://waitingthelongest.com/partners/register",
          unsubscribe_url: "#",
        });
        await sendEmail({
          to: params.from_email,
          ...rendered,
          categories: ["outreach", "response", "interested"],
        });
      }
      action_taken = "sent_registration_link";
      break;
    }

    case "unsubscribe":
    case "not_interested": {
      // Respect the opt-out
      if (params.shelter_id) {
        // Mark contact as opted out
        await supabase
          .from("shelter_contacts")
          .update({ is_opted_out: true, opted_out_at: new Date().toISOString() })
          .eq("email", params.from_email);

        // Update outreach target
        await supabase
          .from("outreach_targets")
          .update({
            status: "opted_out",
            response_category: category,
            responded_at: new Date().toISOString(),
          })
          .eq("shelter_id", params.shelter_id);
      }
      action_taken = "opted_out";
      break;
    }

    case "bounce": {
      // Mark email as invalid
      if (params.shelter_id) {
        await supabase
          .from("outreach_targets")
          .update({
            status: "bounced",
            response_category: "bounce",
          })
          .eq("shelter_id", params.shelter_id);
      }
      action_taken = "marked_bounced";
      break;
    }

    case "auto_reply": {
      // Ignore auto-replies, don't change status
      action_taken = "ignored_auto_reply";
      break;
    }

    case "questions": {
      // Flag for manual review
      if (params.shelter_id) {
        await supabase
          .from("outreach_targets")
          .update({
            status: "responded",
            response_category: "questions",
            responded_at: new Date().toISOString(),
          })
          .eq("shelter_id", params.shelter_id);
      }
      action_taken = "flagged_for_review";
      break;
    }

    default:
      action_taken = "logged_unknown";
  }

  return { category, action_taken };
}
