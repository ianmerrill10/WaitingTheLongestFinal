import { NextRequest, NextResponse } from "next/server";
import {
  generateContent,
  type Platform,
  type ContentType,
} from "@/lib/social/content-generator";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

const VALID_PLATFORMS: Platform[] = [
  "tiktok", "instagram", "facebook", "youtube",
  "reddit", "twitter", "bluesky", "threads", "snapchat",
];
const VALID_TYPES: ContentType[] = [
  "dog_spotlight", "urgent_alert", "happy_tails", "did_you_know",
  "breed_facts", "weekly_roundup", "longest_waiting", "new_arrivals",
  "foster_appeal", "shelter_spotlight",
];

/**
 * POST /api/social/generate — Generate social media content
 */
export async function POST(req: NextRequest) {
  const authHeader = req.headers.get("authorization");
  const cronSecret = process.env.CRON_SECRET;
  if (!cronSecret || authHeader !== `Bearer ${cronSecret}`) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  try {
    const body = await req.json();
    const platform = body.platform as Platform;
    const contentType = body.content_type as ContentType;

    if (!platform || !VALID_PLATFORMS.includes(platform)) {
      return NextResponse.json(
        { error: `Invalid platform. Valid: ${VALID_PLATFORMS.join(", ")}` },
        { status: 400 }
      );
    }
    if (!contentType || !VALID_TYPES.includes(contentType)) {
      return NextResponse.json(
        { error: `Invalid content_type. Valid: ${VALID_TYPES.join(", ")}` },
        { status: 400 }
      );
    }

    const content = await generateContent(platform, contentType);
    if (!content) {
      return NextResponse.json(
        { error: "No suitable dog found for this content type" },
        { status: 404 }
      );
    }

    return NextResponse.json({ content });
  } catch (err) {
    console.error("Social generate error:", err);
    return NextResponse.json(
      { error: "Failed to generate content" },
      { status: 500 }
    );
  }
}
