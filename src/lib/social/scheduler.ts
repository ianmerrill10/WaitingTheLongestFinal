import { createAdminClient } from "@/lib/supabase/admin";
import type { Platform } from "./content-generator";

interface ScheduledPost {
  id: string;
  platform: string;
  caption: string;
  hashtags: string[];
  media_urls: string[];
  media_type: string;
  scheduled_at: string;
  account_id: string;
}

// Optimal posting times (hour in UTC) per platform
const OPTIMAL_TIMES: Record<string, number[]> = {
  instagram: [14, 17, 20],   // 10am, 1pm, 4pm ET
  facebook: [13, 16, 19],    // 9am, 12pm, 3pm ET
  twitter: [13, 17, 21],     // 9am, 1pm, 5pm ET
  tiktok: [15, 19, 23],      // 11am, 3pm, 7pm ET
  youtube: [16, 20],         // 12pm, 4pm ET
  reddit: [14, 18],          // 10am, 2pm ET
  bluesky: [14, 18],         // 10am, 2pm ET
  threads: [14, 17, 20],     // Same as Instagram
};

/**
 * Find the next optimal posting time for a platform.
 */
export function getNextOptimalTime(platform: Platform): Date {
  const now = new Date();
  const times = OPTIMAL_TIMES[platform] || [14, 18];

  // Find the next available slot today or tomorrow
  for (let dayOffset = 0; dayOffset < 2; dayOffset++) {
    for (const hour of times) {
      const candidate = new Date(now);
      candidate.setUTCDate(candidate.getUTCDate() + dayOffset);
      candidate.setUTCHours(hour, 0, 0, 0);
      if (candidate > now) return candidate;
    }
  }

  // Fallback: tomorrow at first optimal time
  const fallback = new Date(now);
  fallback.setUTCDate(fallback.getUTCDate() + 1);
  fallback.setUTCHours(times[0], 0, 0, 0);
  return fallback;
}

/**
 * Schedule a post for a specific time.
 */
export async function schedulePost(postData: {
  account_id: string;
  platform: Platform;
  content_type: string;
  dog_id?: string;
  shelter_id?: string;
  caption: string;
  hashtags: string[];
  media_urls: string[];
  media_type: string;
  scheduled_at?: string;
  generated_by?: string;
}): Promise<{ id: string } | null> {
  const supabase = createAdminClient();

  const scheduledAt =
    postData.scheduled_at || getNextOptimalTime(postData.platform).toISOString();

  const { data, error } = await supabase
    .from("social_posts")
    .insert({
      account_id: postData.account_id,
      platform: postData.platform,
      content_type: postData.content_type,
      dog_id: postData.dog_id || null,
      shelter_id: postData.shelter_id || null,
      caption: postData.caption,
      hashtags: postData.hashtags,
      media_urls: postData.media_urls,
      media_type: postData.media_type,
      status: "scheduled",
      scheduled_at: scheduledAt,
      generated_by: postData.generated_by || "manual",
    })
    .select("id")
    .single();

  if (error) {
    console.error("Failed to schedule post:", error);
    return null;
  }

  return data;
}

/**
 * Get all posts due for publishing.
 */
export async function getDuePostsForPublishing(): Promise<ScheduledPost[]> {
  const supabase = createAdminClient();

  const { data, error } = await supabase
    .from("social_posts")
    .select("id, platform, caption, hashtags, media_urls, media_type, scheduled_at, account_id")
    .eq("status", "scheduled")
    .lte("scheduled_at", new Date().toISOString())
    .order("scheduled_at", { ascending: true })
    .limit(20);

  if (error) {
    console.error("Failed to get due posts:", error);
    return [];
  }

  return (data || []) as ScheduledPost[];
}

/**
 * Mark a post as posting (in progress).
 */
export async function markPostAsPosting(postId: string): Promise<void> {
  const supabase = createAdminClient();
  await supabase
    .from("social_posts")
    .update({ status: "posting", updated_at: new Date().toISOString() })
    .eq("id", postId);
}

/**
 * Mark a post as successfully posted.
 */
export async function markPostAsPosted(
  postId: string,
  externalPostId: string,
  externalPostUrl?: string
): Promise<void> {
  const supabase = createAdminClient();
  await supabase
    .from("social_posts")
    .update({
      status: "posted",
      posted_at: new Date().toISOString(),
      external_post_id: externalPostId,
      external_post_url: externalPostUrl || null,
      updated_at: new Date().toISOString(),
    })
    .eq("id", postId);
}

/**
 * Mark a post as failed.
 */
export async function markPostAsFailed(
  postId: string,
  errorNote: string
): Promise<void> {
  const supabase = createAdminClient();
  await supabase
    .from("social_posts")
    .update({
      status: "failed",
      notes: errorNote,
      updated_at: new Date().toISOString(),
    })
    .eq("id", postId);
}

/**
 * Get upcoming scheduled posts.
 */
export async function getUpcomingPosts(
  limit: number = 20
): Promise<ScheduledPost[]> {
  const supabase = createAdminClient();

  const { data } = await supabase
    .from("social_posts")
    .select("id, platform, caption, hashtags, media_urls, media_type, scheduled_at, account_id")
    .eq("status", "scheduled")
    .gte("scheduled_at", new Date().toISOString())
    .order("scheduled_at", { ascending: true })
    .limit(limit);

  return (data || []) as ScheduledPost[];
}

/**
 * Cancel a scheduled post.
 */
export async function cancelPost(postId: string): Promise<boolean> {
  const supabase = createAdminClient();

  const { error } = await supabase
    .from("social_posts")
    .update({ status: "archived", updated_at: new Date().toISOString() })
    .eq("id", postId)
    .eq("status", "scheduled");

  return !error;
}
