import { NextRequest, NextResponse } from "next/server";
import { schedulePost, getUpcomingPosts, cancelPost } from "@/lib/social/scheduler";
import type { Platform } from "@/lib/social/content-generator";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

/**
 * GET /api/social/schedule — List upcoming scheduled posts
 */
export async function GET() {
  try {
    const posts = await getUpcomingPosts(50);
    return NextResponse.json({ posts });
  } catch (err) {
    console.error("Social schedule GET error:", err);
    return NextResponse.json({ error: "Failed to fetch schedule" }, { status: 500 });
  }
}

/**
 * POST /api/social/schedule — Schedule a post
 */
export async function POST(req: NextRequest) {
  try {
    const body = await req.json();

    if (!body.account_id || !body.platform || !body.caption) {
      return NextResponse.json(
        { error: "Missing required fields: account_id, platform, caption" },
        { status: 400 }
      );
    }

    const result = await schedulePost({
      account_id: body.account_id,
      platform: body.platform as Platform,
      content_type: body.content_type || "dog_spotlight",
      dog_id: body.dog_id,
      shelter_id: body.shelter_id,
      caption: body.caption,
      hashtags: body.hashtags || [],
      media_urls: body.media_urls || [],
      media_type: body.media_type || "photo",
      scheduled_at: body.scheduled_at,
      generated_by: body.generated_by || "manual",
    });

    if (!result) {
      return NextResponse.json(
        { error: "Failed to schedule post" },
        { status: 500 }
      );
    }

    return NextResponse.json({ success: true, post_id: result.id });
  } catch (err) {
    console.error("Social schedule POST error:", err);
    return NextResponse.json({ error: "Failed to schedule" }, { status: 500 });
  }
}

/**
 * DELETE /api/social/schedule — Cancel a scheduled post
 */
export async function DELETE(req: NextRequest) {
  try {
    const body = await req.json();
    if (!body.post_id) {
      return NextResponse.json(
        { error: "Missing post_id" },
        { status: 400 }
      );
    }

    const success = await cancelPost(body.post_id);
    if (!success) {
      return NextResponse.json(
        { error: "Post not found or already published" },
        { status: 404 }
      );
    }

    return NextResponse.json({ success: true });
  } catch (err) {
    console.error("Social schedule DELETE error:", err);
    return NextResponse.json({ error: "Failed to cancel" }, { status: 500 });
  }
}
