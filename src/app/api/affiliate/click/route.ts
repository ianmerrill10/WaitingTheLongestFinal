import { NextRequest, NextResponse } from "next/server";
import { trackClick } from "@/lib/monetization/click-tracker";
import { getRecommendations } from "@/lib/monetization/breed-recommendations";
import { createAdminClient } from "@/lib/supabase/admin";
import { rateLimit } from "@/lib/utils/rate-limit";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

/**
 * GET /api/affiliate/click?dog_id=... — Get product recommendations for a dog
 */
export async function GET(req: NextRequest) {
  const dogId = req.nextUrl.searchParams.get("dog_id");
  if (!dogId) {
    return NextResponse.json({ error: "Missing dog_id" }, { status: 400 });
  }

  try {
    const supabase = createAdminClient();
    const { data: dog } = await supabase
      .from("dogs")
      .select("id, name, breed_primary, size, age_category, description, urgency_level")
      .eq("id", dogId)
      .single();

    if (!dog) {
      return NextResponse.json({ products: [] });
    }

    const products = await getRecommendations({
      id: dog.id,
      name: dog.name,
      breed: dog.breed_primary,
      size: dog.size,
      age_text: dog.age_category,
      description: dog.description,
      urgency_level: dog.urgency_level,
    });

    return NextResponse.json({ products });
  } catch (err) {
    console.error("Affiliate recommendations error:", err);
    return NextResponse.json({ products: [] });
  }
}

/**
 * POST /api/affiliate/click — Track a click on an affiliate link
 */
export async function POST(req: NextRequest) {
  // Rate limit: 30 click tracks per minute per IP
  const fwd = req.headers.get("x-forwarded-for");
  const clientIp = fwd?.split(",")[0]?.trim() || "unknown";
  const { allowed } = rateLimit(`affiliate:${clientIp}`, 30, 60000);
  if (!allowed) {
    return NextResponse.json({ success: false }, { status: 429 });
  }

  try {
    const body = await req.json();
    const { product_id, dog_id, page_url } = body;

    if (!product_id) {
      return NextResponse.json({ error: "Missing product_id" }, { status: 400 });
    }

    // Get partner_id from product
    const supabase = createAdminClient();
    const { data: product } = await supabase
      .from("affiliate_products")
      .select("partner_id")
      .eq("id", product_id)
      .single();

    const ip =
      req.headers.get("x-forwarded-for")?.split(",")[0]?.trim() ||
      req.headers.get("x-real-ip") ||
      "unknown";
    const userAgent = req.headers.get("user-agent") || "unknown";
    const referrer = req.headers.get("referer") || undefined;

    const clickId = await trackClick({
      product_id,
      partner_id: product?.partner_id || product_id,
      dog_id,
      page_url: page_url || "/",
      ip,
      user_agent: userAgent,
      referrer,
    });

    return NextResponse.json({ success: true, click_id: clickId });
  } catch (err) {
    console.error("Click tracking error:", err);
    // Don't fail the user experience for tracking errors
    return NextResponse.json({ success: false });
  }
}
