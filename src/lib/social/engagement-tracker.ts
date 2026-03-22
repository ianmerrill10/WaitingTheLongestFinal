import { createAdminClient } from "@/lib/supabase/admin";

interface EngagementMetrics {
  likes: number;
  comments: number;
  shares: number;
  views: number;
  saves: number;
  clicks: number;
}

/**
 * Update engagement metrics for a post.
 */
export async function updatePostEngagement(
  postId: string,
  metrics: Partial<EngagementMetrics>
): Promise<void> {
  const supabase = createAdminClient();

  // Get current engagement
  const { data: post } = await supabase
    .from("social_posts")
    .select("engagement")
    .eq("id", postId)
    .single();

  if (!post) return;

  const current = (post.engagement || {}) as EngagementMetrics;
  const updated = { ...current, ...metrics };

  // Compute performance score (engagement rate)
  const totalEngagement =
    (updated.likes || 0) +
    (updated.comments || 0) * 2 +
    (updated.shares || 0) * 3 +
    (updated.saves || 0) * 2;
  const views = updated.views || 1;
  const performanceScore = Math.round((totalEngagement / views) * 10000) / 100;

  await supabase
    .from("social_posts")
    .update({
      engagement: updated,
      performance_score: performanceScore,
      last_engagement_sync: new Date().toISOString(),
      updated_at: new Date().toISOString(),
    })
    .eq("id", postId);
}

/**
 * Get top performing posts.
 */
export async function getTopPosts(
  limit: number = 10,
  days: number = 30
): Promise<
  Array<{
    id: string;
    platform: string;
    content_type: string;
    caption: string;
    engagement: EngagementMetrics;
    performance_score: number;
    posted_at: string;
  }>
> {
  const supabase = createAdminClient();
  const since = new Date(Date.now() - days * 24 * 3600 * 1000).toISOString();

  const { data } = await supabase
    .from("social_posts")
    .select(
      "id, platform, content_type, caption, engagement, performance_score, posted_at"
    )
    .eq("status", "posted")
    .gte("posted_at", since)
    .order("performance_score", { ascending: false })
    .limit(limit);

  return (data || []) as Array<{
    id: string;
    platform: string;
    content_type: string;
    caption: string;
    engagement: EngagementMetrics;
    performance_score: number;
    posted_at: string;
  }>;
}

/**
 * Get aggregate engagement stats across platforms.
 */
export async function getEngagementSummary(days: number = 30): Promise<{
  total_posts: number;
  total_likes: number;
  total_comments: number;
  total_shares: number;
  total_views: number;
  avg_engagement_rate: number;
  by_platform: Record<
    string,
    { posts: number; likes: number; shares: number; avg_score: number }
  >;
  by_content_type: Record<string, { posts: number; avg_score: number }>;
}> {
  const supabase = createAdminClient();
  const since = new Date(Date.now() - days * 24 * 3600 * 1000).toISOString();

  const { data: posts } = await supabase
    .from("social_posts")
    .select("platform, content_type, engagement, performance_score")
    .eq("status", "posted")
    .gte("posted_at", since);

  const result = {
    total_posts: 0,
    total_likes: 0,
    total_comments: 0,
    total_shares: 0,
    total_views: 0,
    avg_engagement_rate: 0,
    by_platform: {} as Record<
      string,
      { posts: number; likes: number; shares: number; avg_score: number }
    >,
    by_content_type: {} as Record<string, { posts: number; avg_score: number }>,
  };

  if (!posts || posts.length === 0) return result;

  let totalScore = 0;

  for (const post of posts) {
    const eng = (post.engagement || {}) as EngagementMetrics;
    result.total_posts++;
    result.total_likes += eng.likes || 0;
    result.total_comments += eng.comments || 0;
    result.total_shares += eng.shares || 0;
    result.total_views += eng.views || 0;
    totalScore += post.performance_score || 0;

    // By platform
    if (!result.by_platform[post.platform]) {
      result.by_platform[post.platform] = {
        posts: 0,
        likes: 0,
        shares: 0,
        avg_score: 0,
      };
    }
    result.by_platform[post.platform].posts++;
    result.by_platform[post.platform].likes += eng.likes || 0;
    result.by_platform[post.platform].shares += eng.shares || 0;
    result.by_platform[post.platform].avg_score +=
      post.performance_score || 0;

    // By content type
    if (!result.by_content_type[post.content_type]) {
      result.by_content_type[post.content_type] = { posts: 0, avg_score: 0 };
    }
    result.by_content_type[post.content_type].posts++;
    result.by_content_type[post.content_type].avg_score +=
      post.performance_score || 0;
  }

  result.avg_engagement_rate =
    Math.round((totalScore / result.total_posts) * 100) / 100;

  // Average the scores
  for (const plat of Object.keys(result.by_platform)) {
    const p = result.by_platform[plat];
    p.avg_score = Math.round((p.avg_score / p.posts) * 100) / 100;
  }
  for (const ct of Object.keys(result.by_content_type)) {
    const c = result.by_content_type[ct];
    c.avg_score = Math.round((c.avg_score / c.posts) * 100) / 100;
  }

  return result;
}

/**
 * Update engagement for a template based on posts using it.
 */
export async function updateTemplateEngagement(
  templateId: string
): Promise<void> {
  const supabase = createAdminClient();

  // This would correlate posts back to templates if we track that association
  // For now, just increment usage count
  await supabase.rpc("increment_field", {
    table_name: "social_templates",
    field_name: "usage_count",
    row_id: templateId,
  });
}
