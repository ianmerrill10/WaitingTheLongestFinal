import { NextRequest, NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";

// Auth enforced by middleware — /api/admin/* requires site_auth cookie

export async function GET(request: NextRequest) {
  try {
    const days = Math.min(
      Math.max(parseInt(request.nextUrl.searchParams.get("days") || "30"), 1),
      365
    );
    const supabase = createAdminClient();

    const since = new Date();
    since.setDate(since.getDate() - days);

    const { data: posts, error } = await supabase
      .from("social_posts")
      .select("*")
      .gte("created_at", since.toISOString())
      .order("created_at", { ascending: false });

    if (error) {
      console.error("Social stats query error:", error);
      return NextResponse.json(
        { error: "Failed to load social stats" },
        { status: 500 }
      );
    }

    const allPosts = posts || [];

    const totalLikes = allPosts.reduce((s, p) => s + (p.likes || 0), 0);
    const totalComments = allPosts.reduce(
      (s, p) => s + (p.comments || 0),
      0
    );
    const totalShares = allPosts.reduce((s, p) => s + (p.shares || 0), 0);
    const totalViews = allPosts.reduce((s, p) => s + (p.views || 0), 0);

    // Aggregate by platform
    const byPlatform: Record<
      string,
      { posts: number; likes: number; shares: number; avg_score: number }
    > = {};
    for (const post of allPosts) {
      const platform = post.platform || "unknown";
      if (!byPlatform[platform]) {
        byPlatform[platform] = { posts: 0, likes: 0, shares: 0, avg_score: 0 };
      }
      byPlatform[platform].posts++;
      byPlatform[platform].likes += post.likes || 0;
      byPlatform[platform].shares += post.shares || 0;
    }
    for (const key of Object.keys(byPlatform)) {
      const p = byPlatform[key];
      p.avg_score =
        p.posts > 0
          ? Math.round(((p.likes + p.shares) / p.posts) * 10) / 10
          : 0;
    }

    // Aggregate by content type
    const byContentType: Record<
      string,
      { posts: number; avg_score: number }
    > = {};
    for (const post of allPosts) {
      const type = post.content_type || "unknown";
      if (!byContentType[type]) {
        byContentType[type] = { posts: 0, avg_score: 0 };
      }
      byContentType[type].posts++;
    }
    for (const key of Object.keys(byContentType)) {
      const ct = byContentType[key];
      const ctPosts = allPosts.filter(
        (p) => (p.content_type || "unknown") === key
      );
      const engagement = ctPosts.reduce(
        (s, p) => s + (p.likes || 0) + (p.shares || 0),
        0
      );
      ct.avg_score =
        ct.posts > 0 ? Math.round((engagement / ct.posts) * 10) / 10 : 0;
    }

    const avgEngagement =
      allPosts.length > 0
        ? Math.round(
            ((totalLikes + totalComments + totalShares) / allPosts.length) * 10
          ) / 10
        : 0;

    return NextResponse.json({
      total_posts: allPosts.length,
      total_likes: totalLikes,
      total_comments: totalComments,
      total_shares: totalShares,
      total_views: totalViews,
      avg_engagement_rate: avgEngagement,
      by_platform: byPlatform,
      by_content_type: byContentType,
    });
  } catch (err) {
    console.error("Admin social stats API error:", err);
    return NextResponse.json(
      { error: "Failed to load social stats" },
      { status: 500 }
    );
  }
}
