/**
 * Email templates for shelter outreach campaigns.
 * All templates include CAN-SPAM compliance (unsubscribe, physical address).
 */

export interface EmailTemplate {
  id: string;
  name: string;
  subject: string;
  html: string;
  text: string;
  stage: "initial" | "follow_up_1" | "follow_up_2" | "follow_up_3" | "welcome" | "integration" | "monthly_report";
}

// Variables available in all templates:
// {{shelter_name}}, {{contact_name}}, {{contact_first_name}}
// {{city}}, {{state}}, {{dog_count}}, {{top_breed}}
// {{shelter_url}}, {{register_url}}, {{unsubscribe_url}}
// {{view_count}}, {{interest_count}}

const FOOTER = `
<hr style="border:none;border-top:1px solid #e5e7eb;margin:32px 0 16px">
<p style="font-size:12px;color:#9ca3af;line-height:1.5">
  WaitingTheLongest.com — Stop a Clock, Save a Life<br>
  Every shelter. Every dog. Free forever.<br><br>
  <a href="{{unsubscribe_url}}" style="color:#9ca3af">Unsubscribe</a> from future emails.
</p>`;

const TEXT_FOOTER = `
---
WaitingTheLongest.com — Stop a Clock, Save a Life
Every shelter. Every dog. Free forever.

Unsubscribe: {{unsubscribe_url}}`;

export const TEMPLATES: EmailTemplate[] = [
  {
    id: "initial_outreach",
    name: "Initial Outreach",
    subject: "Your dogs are already on WaitingTheLongest.com",
    stage: "initial",
    html: `
<div style="font-family:system-ui,sans-serif;max-width:600px;margin:0 auto;padding:20px">
  <p>Hi {{contact_first_name}},</p>

  <p>I'm reaching out because <strong>{{shelter_name}}</strong> in {{city}}, {{state}} already has <strong>{{dog_count}} dogs</strong> listed on WaitingTheLongest.com — a free platform that shows every adoptable dog in America with real-time countdown timers showing how long they've been waiting.</p>

  <p>We pulled your listings from RescueGroups automatically, but I wanted to let you know that you can <strong>take full control of your listings for free</strong>. As a registered partner, you get:</p>

  <ul>
    <li><strong>Full API access</strong> — sync your dogs automatically via API, webhook, CSV, or manual upload</li>
    <li><strong>Real-time analytics</strong> — see which dogs get the most views and interest</li>
    <li><strong>Priority placement</strong> — verified partner shelters get highlighted in search results</li>
    <li><strong>Custom branding</strong> — your shelter page with your logo and contact info</li>
    <li><strong>Zero cost</strong> — everything is free, forever. No ads on urgent dog pages.</li>
  </ul>

  <p>It takes about 2 minutes to register:</p>

  <p style="text-align:center;margin:24px 0">
    <a href="{{register_url}}" style="background:#15803d;color:white;padding:12px 32px;border-radius:8px;text-decoration:none;font-weight:bold;display:inline-block">Register for Free</a>
  </p>

  <p>You can also see your current listings here: <a href="{{shelter_url}}">{{shelter_url}}</a></p>

  <p>Thank you for the work you do saving lives. If you have any questions, just reply to this email.</p>

  <p>Best,<br>The WaitingTheLongest.com Team</p>
  ${FOOTER}
</div>`,
    text: `Hi {{contact_first_name}},

I'm reaching out because {{shelter_name}} in {{city}}, {{state}} already has {{dog_count}} dogs listed on WaitingTheLongest.com — a free platform that shows every adoptable dog in America with real-time countdown timers.

We pulled your listings automatically, but you can take full control for free. As a registered partner, you get:

- Full API access — sync dogs automatically
- Real-time analytics — see views and interest
- Priority placement in search results
- Custom branding on your shelter page
- Zero cost — free forever

Register in 2 minutes: {{register_url}}

See your current listings: {{shelter_url}}

Thank you for saving lives.

The WaitingTheLongest.com Team
${TEXT_FOOTER}`,
  },

  {
    id: "follow_up_1",
    name: "Follow-Up 1 (Day 3)",
    subject: "Quick follow-up about your shelter dogs on WaitingTheLongest.com",
    stage: "follow_up_1",
    html: `
<div style="font-family:system-ui,sans-serif;max-width:600px;margin:0 auto;padding:20px">
  <p>Hi {{contact_first_name}},</p>

  <p>I wanted to follow up on my earlier email about {{shelter_name}}'s dogs on WaitingTheLongest.com.</p>

  <p>Since I last wrote, your dogs have been viewed <strong>{{view_count}} times</strong> by potential adopters. That's {{view_count}} people who saw your dogs and might be the perfect match.</p>

  <p>By registering as a partner (free), you can:</p>
  <ul>
    <li>See exactly which dogs get the most attention</li>
    <li>Update listings in real-time when dogs get adopted</li>
    <li>Receive notifications when someone expresses interest</li>
  </ul>

  <p style="text-align:center;margin:24px 0">
    <a href="{{register_url}}" style="background:#15803d;color:white;padding:12px 32px;border-radius:8px;text-decoration:none;font-weight:bold;display:inline-block">Register for Free</a>
  </p>

  <p>No strings attached. Just reply if you have questions.</p>

  <p>Best,<br>The WaitingTheLongest.com Team</p>
  ${FOOTER}
</div>`,
    text: `Hi {{contact_first_name}},

Following up on my earlier email about {{shelter_name}}'s dogs on WaitingTheLongest.com.

Since I last wrote, your dogs have been viewed {{view_count}} times by potential adopters.

Register for free to see analytics, update listings, and get interest notifications: {{register_url}}

No strings attached. Reply with any questions.

The WaitingTheLongest.com Team
${TEXT_FOOTER}`,
  },

  {
    id: "follow_up_2",
    name: "Follow-Up 2 (Day 7)",
    subject: "{{dog_count}} dogs from {{shelter_name}} — free tools available",
    stage: "follow_up_2",
    html: `
<div style="font-family:system-ui,sans-serif;max-width:600px;margin:0 auto;padding:20px">
  <p>Hi {{contact_first_name}},</p>

  <p>Quick update: <strong>{{dog_count}} dogs</strong> from {{shelter_name}} are currently being showcased to adopters across America on WaitingTheLongest.com.</p>

  <p>I know you're busy — shelter work never stops. That's exactly why we built this to be as simple as possible:</p>

  <ol>
    <li>Register (2 minutes): <a href="{{register_url}}">{{register_url}}</a></li>
    <li>Your existing listings are already there</li>
    <li>Optional: set up API sync so new dogs appear automatically</li>
  </ol>

  <p>We've had shelters tell us they saw increased adoption inquiries within a week of registering. Would love for {{shelter_name}} to see the same results.</p>

  <p>Best,<br>The WaitingTheLongest.com Team</p>
  ${FOOTER}
</div>`,
    text: `Hi {{contact_first_name}},

{{dog_count}} dogs from {{shelter_name}} are being showcased on WaitingTheLongest.com.

3 simple steps:
1. Register (2 min): {{register_url}}
2. Your listings are already there
3. Optional: set up automatic sync

We'd love for {{shelter_name}} to benefit from increased visibility.

The WaitingTheLongest.com Team
${TEXT_FOOTER}`,
  },

  {
    id: "follow_up_3",
    name: "Follow-Up 3 (Day 14) — Final",
    subject: "Last check-in — free tools for {{shelter_name}}",
    stage: "follow_up_3",
    html: `
<div style="font-family:system-ui,sans-serif;max-width:600px;margin:0 auto;padding:20px">
  <p>Hi {{contact_first_name}},</p>

  <p>This is my final follow-up. I completely understand if {{shelter_name}} isn't interested right now — your focus should be on the dogs, not on emails from platforms.</p>

  <p>Just know that your dogs will continue to be listed on WaitingTheLongest.com for free regardless. We're here because we believe every dog deserves to be seen.</p>

  <p>If you ever want to take control of your listings, the offer stands: <a href="{{register_url}}">Register anytime</a></p>

  <p>Thank you for saving lives every day.</p>

  <p>Best,<br>The WaitingTheLongest.com Team</p>
  ${FOOTER}
</div>`,
    text: `Hi {{contact_first_name}},

This is my final follow-up. I understand if {{shelter_name}} isn't interested right now.

Your dogs will continue to be listed for free regardless. We're here because every dog deserves to be seen.

Register anytime: {{register_url}}

Thank you for saving lives every day.

The WaitingTheLongest.com Team
${TEXT_FOOTER}`,
  },

  {
    id: "welcome_partner",
    name: "Welcome Partner",
    subject: "Welcome to WaitingTheLongest.com, {{shelter_name}}!",
    stage: "welcome",
    html: `
<div style="font-family:system-ui,sans-serif;max-width:600px;margin:0 auto;padding:20px">
  <p>Hi {{contact_first_name}},</p>

  <p>Welcome to WaitingTheLongest.com! 🎉 {{shelter_name}} is now a registered partner.</p>

  <p><strong>Here's what you can do now:</strong></p>

  <ol>
    <li><strong>Log in to your dashboard:</strong> <a href="https://waitingthelongest.com/partners/login">waitingthelongest.com/partners/login</a></li>
    <li><strong>Review your dog listings</strong> and correct any inaccurate information</li>
    <li><strong>Set up API integration</strong> for automatic syncing (or upload via CSV)</li>
    <li><strong>Configure webhooks</strong> to get notified of interest and views</li>
  </ol>

  <p><strong>Your API key</strong> has been emailed separately. Keep it secure.</p>

  <p>Need help? Just reply to this email or check our <a href="https://waitingthelongest.com/legal/api-terms">API documentation</a>.</p>

  <p>Together, we're going to help more dogs find homes. Thank you for joining us.</p>

  <p>The WaitingTheLongest.com Team</p>
  ${FOOTER}
</div>`,
    text: `Hi {{contact_first_name}},

Welcome to WaitingTheLongest.com! {{shelter_name}} is now a registered partner.

What you can do now:
1. Log in: waitingthelongest.com/partners/login
2. Review your dog listings
3. Set up API integration
4. Configure webhooks

Your API key has been emailed separately.

Need help? Reply to this email.

The WaitingTheLongest.com Team
${TEXT_FOOTER}`,
  },
];

/**
 * Get a template by ID.
 */
export function getTemplate(id: string): EmailTemplate | undefined {
  return TEMPLATES.find((t) => t.id === id);
}

/**
 * Render a template with variables.
 */
export function renderTemplate(
  template: EmailTemplate,
  vars: Record<string, string>
): { subject: string; html: string; text: string } {
  let subject = template.subject;
  let html = template.html;
  let text = template.text;

  for (const [key, value] of Object.entries(vars)) {
    const regex = new RegExp(`\\{\\{${key}\\}\\}`, "g");
    subject = subject.replace(regex, value);
    html = html.replace(regex, value);
    text = text.replace(regex, value);
  }

  return { subject, html, text };
}
