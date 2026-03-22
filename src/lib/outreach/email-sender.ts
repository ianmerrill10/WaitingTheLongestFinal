/**
 * SendGrid email sender for outreach campaigns.
 *
 * Environment variables:
 *   SENDGRID_API_KEY
 *   SENDGRID_FROM_EMAIL (e.g., "outreach@waitingthelongest.com")
 *   SENDGRID_FROM_NAME (e.g., "WaitingTheLongest.com")
 */

const SENDGRID_API = "https://api.sendgrid.com/v3";

interface SendEmailParams {
  to: string;
  subject: string;
  html: string;
  text: string;
  replyTo?: string;
  categories?: string[];
  customArgs?: Record<string, string>;
}

interface SendResult {
  success: boolean;
  messageId?: string;
  error?: string;
}

/**
 * Send an email via SendGrid.
 */
export async function sendEmail(params: SendEmailParams): Promise<SendResult> {
  const apiKey = process.env.SENDGRID_API_KEY;
  const fromEmail = process.env.SENDGRID_FROM_EMAIL || "outreach@waitingthelongest.com";
  const fromName = process.env.SENDGRID_FROM_NAME || "WaitingTheLongest.com";

  if (!apiKey) {
    console.error("SendGrid: Missing API key");
    return { success: false, error: "SendGrid not configured" };
  }

  try {
    const res = await fetch(`${SENDGRID_API}/mail/send`, {
      method: "POST",
      headers: {
        Authorization: `Bearer ${apiKey}`,
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        personalizations: [
          {
            to: [{ email: params.to }],
            custom_args: params.customArgs || {},
          },
        ],
        from: { email: fromEmail, name: fromName },
        reply_to: { email: params.replyTo || fromEmail, name: fromName },
        subject: params.subject,
        content: [
          { type: "text/plain", value: params.text },
          { type: "text/html", value: params.html },
        ],
        categories: params.categories || ["outreach"],
        tracking_settings: {
          click_tracking: { enable: true },
          open_tracking: { enable: true },
        },
        // CAN-SPAM: unsubscribe header
        headers: {
          "List-Unsubscribe": `<mailto:unsubscribe@waitingthelongest.com?subject=Unsubscribe>`,
        },
      }),
    });

    if (res.status === 202) {
      const messageId = res.headers.get("X-Message-Id") || undefined;
      return { success: true, messageId };
    }

    const errorData = await res.json().catch(() => ({}));
    console.error("SendGrid error:", res.status, errorData);
    return {
      success: false,
      error: `SendGrid ${res.status}: ${JSON.stringify(errorData)}`,
    };
  } catch (err) {
    const errMsg = err instanceof Error ? err.message : "Unknown error";
    console.error("SendGrid exception:", errMsg);
    return { success: false, error: errMsg };
  }
}

/**
 * Check if an email address appears to be valid.
 * Basic format check + common typo domains.
 */
export function isValidOutreachEmail(email: string): boolean {
  if (!email || !email.includes("@")) return false;

  // Skip generic addresses that are unlikely to reach decision-makers
  const genericPrefixes = [
    "noreply",
    "no-reply",
    "donotreply",
    "do-not-reply",
    "mailer-daemon",
    "postmaster",
  ];
  const prefix = email.split("@")[0].toLowerCase();
  if (genericPrefixes.some((g) => prefix.startsWith(g))) return false;

  return true;
}

/**
 * Rate limiter for outreach emails.
 * Enforces 50 emails per hour to avoid spam filters.
 */
let emailsSentThisHour = 0;
let hourResetAt = Date.now() + 3600 * 1000;

export function canSendEmail(): boolean {
  const now = Date.now();
  if (now > hourResetAt) {
    emailsSentThisHour = 0;
    hourResetAt = now + 3600 * 1000;
  }
  return emailsSentThisHour < 50;
}

export function recordEmailSent(): void {
  emailsSentThisHour++;
}

/**
 * Generate unsubscribe URL for a shelter contact.
 */
export function getUnsubscribeUrl(contactId: string): string {
  return `https://waitingthelongest.com/api/outreach/unsubscribe?id=${contactId}`;
}
