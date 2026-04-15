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
import { parseCsvDogs } from "@/lib/shelter-api/csv-parser";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

/**
 * POST /api/v1/dogs/import — Import dogs from CSV text
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

  let csvText: string;
  const contentType = request.headers.get("content-type") || "";

  if (contentType.includes("text/csv") || contentType.includes("text/plain")) {
    csvText = await request.text();
  } else if (contentType.includes("application/json")) {
    try {
      const body = await request.json();
      if (!body.csv || typeof body.csv !== "string") {
        return NextResponse.json(
          { error: "JSON body must contain a 'csv' string field", code: "VALIDATION_ERROR" },
          { status: 400, headers: rateLimitHeaders(rateLimit) }
        );
      }
      csvText = body.csv;
    } catch {
      return NextResponse.json(
        { error: "Invalid request body", code: "INVALID_BODY" },
        { status: 400, headers: rateLimitHeaders(rateLimit) }
      );
    }
  } else {
    return NextResponse.json(
      {
        error: "Content-Type must be text/csv, text/plain, or application/json",
        code: "INVALID_CONTENT_TYPE",
      },
      { status: 415, headers: rateLimitHeaders(rateLimit) }
    );
  }

  // Enforce CSV size limit to prevent DoS
  const MAX_CSV_SIZE = 5 * 1024 * 1024; // 5MB
  if (csvText.length > MAX_CSV_SIZE) {
    return NextResponse.json(
      { error: `CSV too large. Maximum size is 5MB.`, code: "PAYLOAD_TOO_LARGE" },
      { status: 413, headers: rateLimitHeaders(rateLimit) }
    );
  }

  // Parse CSV
  const parseResult = parseCsvDogs(csvText);

  if (parseResult.errors.length > 0 && parseResult.valid_rows === 0) {
    return NextResponse.json(
      {
        error: "CSV parsing failed — no valid rows",
        parse_errors: parseResult.errors,
        code: "PARSE_ERROR",
      },
      { status: 400, headers: rateLimitHeaders(rateLimit) }
    );
  }

  // Insert valid dogs
  const supabase = createAdminClient();
  let created = 0;
  const insertErrors: Array<{ row: number; name: string; error: string }> = [];

  const importTimestamp = new Date().toISOString();
  for (let i = 0; i < parseResult.dogs.length; i++) {
    const dog = parseResult.dogs[i];
    // Every imported dog gets a source_link documenting the CSV provenance
    const sourceLinks = [
      {
        url: dog.external_url || "",
        source: "csv_import",
        checked_at: importTimestamp,
        status_code: 200,
        description: `CSV import via partner API key (row ${i + 2})${dog.intake_date ? " with intake_date" : ""}`,
      },
    ];
    const { error } = await supabase.from("dogs").insert({
      name: dog.name,
      shelter_id: auth.shelter_id,
      breed_primary: dog.breed,
      sex: dog.sex,
      size_general: dog.size,
      color_primary: dog.color,
      size_current_lbs: dog.weight,
      description: dog.description,
      photo_url: dog.photo_url,
      photo_urls: dog.additional_photos.length > 0
        ? [dog.photo_url, ...dog.additional_photos].filter(Boolean)
        : dog.photo_url ? [dog.photo_url] : null,
      is_available: dog.status === "available",
      status: dog.status,
      intake_date: dog.intake_date || new Date().toISOString(),
      adoption_fee: dog.adoption_fee,
      is_house_trained: dog.is_house_trained,
      good_with_kids: dog.good_with_kids,
      good_with_dogs: dog.good_with_dogs,
      good_with_cats: dog.good_with_cats,
      special_needs: dog.special_needs,
      age_text: dog.age_text,
      external_id: dog.external_id,
      external_url: dog.external_url,
      external_source: "csv_import",
      date_confidence: dog.intake_date ? "verified" : "low",
      date_source: "csv_import",
      source_extraction_method: "partner_csv_import",
      original_intake_date: dog.intake_date || importTimestamp,
      ranking_eligible: !!dog.intake_date,
      intake_date_observation_count: 1,
      source_links: sourceLinks,
    });

    if (error) {
      console.error(`[ImportAPI] Insert error row ${i + 2}:`, error.message);
      insertErrors.push({ row: i + 2, name: dog.name, error: "Failed to insert" });
    } else {
      created++;
    }
  }

  await logApiRequest({
    shelter_id: auth.shelter_id!,
    api_key_id: auth.key!.id,
    method: "POST",
    path: "/api/v1/dogs/import",
    status_code: insertErrors.length > 0 ? 207 : 201,
    response_time_ms: Date.now() - startTime,
  });

  return NextResponse.json(
    {
      message: `CSV import complete: ${created} dogs created`,
      total_rows: parseResult.total_rows,
      valid_rows: parseResult.valid_rows,
      created,
      parse_errors: parseResult.errors,
      parse_warnings: parseResult.warnings,
      insert_errors: insertErrors,
    },
    {
      status: insertErrors.length > 0 || parseResult.errors.length > 0 ? 207 : 201,
      headers: rateLimitHeaders(rateLimit),
    }
  );
}
