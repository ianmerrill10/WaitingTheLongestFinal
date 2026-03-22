import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { submitApplication } from "@/lib/shelter-crm/onboarding";
import { createAgreement } from "@/lib/shelter-crm/agreements";
import { rateLimit } from "@/lib/utils/rate-limit";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

export async function POST(request: Request) {
  // Rate limit registrations (5/min per IP)
  const ip = request.headers.get("x-forwarded-for")?.split(",")[0]?.trim() || "unknown";
  const { allowed } = rateLimit(ip, 5, 60000);
  if (!allowed) {
    return NextResponse.json(
      { error: "Too many registration attempts. Please try again later." },
      { status: 429 }
    );
  }

  let body;
  try {
    body = await request.json();
  } catch {
    return NextResponse.json(
      { error: "Invalid request body" },
      { status: 400 }
    );
  }

  // Required field validation
  const required = [
    "org_name",
    "org_type",
    "contact_first_name",
    "contact_last_name",
    "contact_email",
    "city",
    "state_code",
    "zip_code",
  ];

  for (const field of required) {
    if (!body[field] || typeof body[field] !== "string" || !body[field].trim()) {
      return NextResponse.json(
        { error: `${field.replace(/_/g, " ")} is required` },
        { status: 400 }
      );
    }
  }

  // Email validation
  const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
  if (!emailRegex.test(body.contact_email)) {
    return NextResponse.json(
      { error: "Invalid email address" },
      { status: 400 }
    );
  }

  // State code validation
  if (body.state_code.length !== 2) {
    return NextResponse.json(
      { error: "Invalid state code" },
      { status: 400 }
    );
  }

  // Org type validation
  const validOrgTypes = [
    "municipal",
    "private",
    "rescue",
    "foster_network",
    "humane_society",
    "spca",
  ];
  if (!validOrgTypes.includes(body.org_type)) {
    return NextResponse.json(
      { error: "Invalid organization type" },
      { status: 400 }
    );
  }

  try {
    // Submit the onboarding application
    const onboarding = await submitApplication({
      org_name: body.org_name.trim(),
      org_type: body.org_type,
      ein: body.ein?.trim() || undefined,
      website: body.website?.trim() || undefined,
      contact_first_name: body.contact_first_name.trim(),
      contact_last_name: body.contact_last_name.trim(),
      contact_email: body.contact_email.trim().toLowerCase(),
      contact_phone: body.contact_phone?.trim() || undefined,
      contact_title: body.contact_title?.trim() || undefined,
      address: body.address?.trim() || undefined,
      city: body.city.trim(),
      state_code: body.state_code.toUpperCase(),
      zip_code: body.zip_code.trim(),
      estimated_dog_count: body.estimated_dog_count || undefined,
      current_software: body.current_software || undefined,
      data_feed_url: body.data_feed_url?.trim() || undefined,
      social_facebook: body.social_facebook?.trim() || undefined,
      social_instagram: body.social_instagram?.trim() || undefined,
      social_tiktok: body.social_tiktok?.trim() || undefined,
      integration_preference: body.integration_preference || "api",
      referral_source: body.referral_source?.trim() || undefined,
    });

    // Record agreement acceptances if we have a matched shelter
    if (onboarding.shelter_id) {
      const ip =
        request.headers.get("x-forwarded-for")?.split(",")[0]?.trim() ||
        "unknown";
      const userAgent = request.headers.get("user-agent") || "";
      const acceptorName = `${body.contact_first_name.trim()} ${body.contact_last_name.trim()}`;
      const acceptorEmail = body.contact_email.trim().toLowerCase();

      const agreementPromises = [
        createAgreement({
          shelter_id: onboarding.shelter_id,
          document_type: "terms_of_service",
          document_version: "1.0",
          accepted_by_name: acceptorName,
          accepted_by_email: acceptorEmail,
          accepted_by_title: body.contact_title?.trim(),
          accepted_by_ip: ip,
          accepted_by_user_agent: userAgent,
        }),
        createAgreement({
          shelter_id: onboarding.shelter_id,
          document_type: "data_sharing_agreement",
          document_version: "1.0",
          accepted_by_name: acceptorName,
          accepted_by_email: acceptorEmail,
          accepted_by_title: body.contact_title?.trim(),
          accepted_by_ip: ip,
          accepted_by_user_agent: userAgent,
        }),
        createAgreement({
          shelter_id: onboarding.shelter_id,
          document_type: "privacy_policy",
          document_version: "1.0",
          accepted_by_name: acceptorName,
          accepted_by_email: acceptorEmail,
          accepted_by_title: body.contact_title?.trim(),
          accepted_by_ip: ip,
          accepted_by_user_agent: userAgent,
        }),
      ];

      await Promise.allSettled(agreementPromises);
    }

    return NextResponse.json({
      success: true,
      application_id: onboarding.id,
      matched_shelter: !!onboarding.shelter_id,
    });
  } catch (err) {
    const message =
      err instanceof Error ? err.message : "Registration failed";
    console.error("Partner registration error:", message);
    return NextResponse.json({ error: message }, { status: 500 });
  }
}
