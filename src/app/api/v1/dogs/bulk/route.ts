import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import {
  authenticateApiKey,
  authError,
  hasScope,
  scopeError,
  logApiRequest,
} from "@/lib/shelter-api/auth";
import {
  checkApiKeyRateLimit,
  rateLimitHeaders,
} from "@/lib/shelter-api/rate-limiter";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

/**
 * POST /api/v1/dogs/bulk — Bulk create/update dogs from JSON array
 */
export async function POST(request: Request) {
  const startTime = Date.now();
  const auth = await authenticateApiKey(request);
  if (!auth.authenticated) return authError(auth);

  const rateLimit = checkApiKeyRateLimit(auth.key!);
  if (!rateLimit.allowed) {
    return NextResponse.json(
      { error: "Rate limit exceeded", code: "RATE_LIMITED" },
      { status: 429, headers: rateLimitHeaders(rateLimit) }
    );
  }

  if (!hasScope(auth.key!, "bulk_import")) {
    return scopeError("bulk_import");
  }

  let body;
  try {
    body = await request.json();
  } catch {
    return NextResponse.json(
      { error: "Invalid JSON body", code: "INVALID_BODY" },
      { status: 400, headers: rateLimitHeaders(rateLimit) }
    );
  }

  if (!Array.isArray(body.dogs)) {
    return NextResponse.json(
      { error: "Request body must contain a 'dogs' array", code: "VALIDATION_ERROR" },
      { status: 400, headers: rateLimitHeaders(rateLimit) }
    );
  }

  if (body.dogs.length > 500) {
    return NextResponse.json(
      { error: "Maximum 500 dogs per bulk request", code: "VALIDATION_ERROR" },
      { status: 400, headers: rateLimitHeaders(rateLimit) }
    );
  }

  const supabase = createAdminClient();
  const results = {
    created: 0,
    updated: 0,
    errors: [] as Array<{ index: number; name: string; error: string }>,
  };

  for (let i = 0; i < body.dogs.length; i++) {
    const dog = body.dogs[i];

    if (!dog.name) {
      results.errors.push({ index: i, name: "", error: "name is required" });
      continue;
    }

    const hasProvidedIntakeDate = typeof dog.intake_date === "string" && dog.intake_date.trim().length > 0;
    const intakeDate = hasProvidedIntakeDate ? dog.intake_date : new Date().toISOString();

    // Every partner-submitted dog gets a source_link for provenance
    const submissionTimestamp = new Date().toISOString();
    const sourceLinks = [
      {
        url: dog.external_url || "",
        source: "partner_api",
        checked_at: submissionTimestamp,
        status_code: 200,
        description: `Partner API bulk submission${hasProvidedIntakeDate ? " with intake_date" : ""}`,
      },
    ];

    const baseDogData = {
      name: dog.name.trim(),
      shelter_id: auth.shelter_id,
      breed_primary: dog.breed || dog.breed_primary || null,
      breed_secondary: dog.breed_secondary || null,
      age_text: dog.age_text || dog.age || null,
      sex: dog.sex || null,
      size_general: dog.size || dog.size_general || null,
      size_current_lbs: dog.weight || dog.size_current_lbs || null,
      color_primary: dog.color || dog.color_primary || null,
      description: dog.description || null,
      photo_url: dog.photo_url || null,
      photo_urls: dog.photos || dog.photo_urls || null,
      is_available: dog.status !== "adopted",
      status: dog.status || "available",
      adoption_fee: dog.adoption_fee || null,
      is_house_trained: dog.is_house_trained ?? null,
      good_with_kids: dog.good_with_kids ?? null,
      good_with_dogs: dog.good_with_dogs ?? null,
      good_with_cats: dog.good_with_cats ?? null,
      special_needs: dog.special_needs || null,
      external_id: dog.external_id || null,
      external_url: dog.external_url || null,
      external_source: "api_direct",
      source_extraction_method: "partner_api_direct",
    };

    const createDogData = {
      ...baseDogData,
      intake_date: intakeDate,
      date_confidence: hasProvidedIntakeDate ? "verified" : "low",
      date_source: hasProvidedIntakeDate ? "api_submission" : "api_submission_missing_intake_date",
      original_intake_date: intakeDate,
      ranking_eligible: hasProvidedIntakeDate,
      intake_date_observation_count: 1,
      source_links: sourceLinks,
    };

    try {
      if (dog.external_id) {
        // Try to update existing dog by external_id
        const { data: existing } = await supabase
          .from("dogs")
          .select("id, intake_date, original_intake_date")
          .eq("shelter_id", auth.shelter_id!)
          .eq("external_id", dog.external_id)
          .single();

        if (existing) {
          const updateDogData: Record<string, unknown> = { ...baseDogData };
          if (hasProvidedIntakeDate) {
            updateDogData.intake_date = intakeDate;
            updateDogData.date_confidence = "verified";
            updateDogData.date_source = "api_submission";
            updateDogData.original_intake_date = existing.original_intake_date || existing.intake_date;
            updateDogData.ranking_eligible = true;
            updateDogData.intake_date_observation_count = 1;
          }

          const { error } = await supabase
            .from("dogs")
            .update(updateDogData)
            .eq("id", existing.id);

          if (error) {
            console.error(`[BulkAPI] Update error for dog ${i}:`, error.message);
            results.errors.push({ index: i, name: dog.name, error: "Failed to update" });
          } else {
            results.updated++;
          }
          continue;
        }
      }

      // Create new
      const { error } = await supabase.from("dogs").insert(createDogData);
      if (error) {
        console.error(`[BulkAPI] Insert error for dog ${i}:`, error.message);
        results.errors.push({ index: i, name: dog.name, error: "Failed to create" });
      } else {
        results.created++;
      }
    } catch (err) {
      results.errors.push({
        index: i,
        name: dog.name,
        error: err instanceof Error ? err.message : "Unknown error",
      });
    }
  }

  await logApiRequest({
    shelter_id: auth.shelter_id!,
    api_key_id: auth.key!.id,
    method: "POST",
    path: "/api/v1/dogs/bulk",
    status_code: results.errors.length > 0 ? 207 : 201,
    response_time_ms: Date.now() - startTime,
  });

  return NextResponse.json(
    {
      message: `Bulk operation complete: ${results.created} created, ${results.updated} updated, ${results.errors.length} errors`,
      ...results,
    },
    {
      status: results.errors.length > 0 ? 207 : 201,
      headers: rateLimitHeaders(rateLimit),
    }
  );
}
