import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";
import { DOGS_PER_PAGE } from "@/lib/constants";
import { rateLimit } from "@/lib/utils/rate-limit";

import { getDogs } from "@/lib/dog-queries";

export const dynamic = "force-dynamic";

export async function GET(request: Request) {
  try {
  const forwarded = request.headers.get("x-forwarded-for");
  const ip = forwarded?.split(",")[0]?.trim() || "unknown";
  const { allowed } = rateLimit(ip, 120, 60000);
  if (!allowed) {
    return NextResponse.json(
      { error: "Too many requests. Please try again later." },
      { status: 429, headers: { "Retry-After": "60", "X-RateLimit-Remaining": "0" } }
    );
  }

  const { searchParams } = new URL(request.url);
  const page = Math.max(1, parseInt(searchParams.get("page") || "1") || 1);
  const limit = Math.min(Math.max(1, parseInt(searchParams.get("limit") || String(DOGS_PER_PAGE)) || DOGS_PER_PAGE), 100);

  const supabase = createAdminClient();
  const query = getDogs(supabase, searchParams);
  const { data, error, count } = await query;

  if (error) {
    console.error("[DogsAPI] Query error:", error.message);
    return NextResponse.json({ error: "Failed to fetch dogs" }, { status: 500 });
  }

  return NextResponse.json({
    dogs: data || [],
    total: count || 0,
    page,
    limit,
    totalPages: Math.ceil((count || 0) / limit),
  });
  } catch (err) {
    console.error("[DogsAPI] Unhandled error:", err);
    return NextResponse.json({ error: "Failed to fetch dogs" }, { status: 500 });
  }
}
