/**
 * TikTok for Developers API client.
 *
 * Requires: TikTok Developer App
 * API: TikTok Content Posting API
 * Auth: OAuth 2.0
 *
 * Environment variables:
 *   TIKTOK_CLIENT_KEY
 *   TIKTOK_CLIENT_SECRET
 *   TIKTOK_ACCESS_TOKEN
 */

const API_BASE = "https://open.tiktokapis.com/v2";

interface TikTokPostResult {
  publish_id: string;
  post_url: string | null;
}

/**
 * Initialize a photo post on TikTok.
 * TikTok requires uploading via their CDN first.
 */
export async function publishPhotoPost(params: {
  photoUrls: string[];
  caption: string;
}): Promise<TikTokPostResult | null> {
  const token = process.env.TIKTOK_ACCESS_TOKEN;
  if (!token) {
    console.error("TikTok: Missing ACCESS_TOKEN");
    return null;
  }

  try {
    const res = await fetch(`${API_BASE}/post/publish/content/init/`, {
      method: "POST",
      headers: {
        Authorization: `Bearer ${token}`,
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        post_info: {
          title: params.caption.slice(0, 150),
          privacy_level: "PUBLIC_TO_EVERYONE",
          disable_duet: false,
          disable_stitch: false,
          disable_comment: false,
        },
        source_info: {
          source: "PULL_FROM_URL",
          photo_cover_index: 0,
          photo_images: params.photoUrls,
        },
        media_type: "PHOTO",
      }),
    });

    const data = await res.json();
    if (!res.ok || data.error?.code) {
      console.error("TikTok publish error:", data);
      return null;
    }

    return {
      publish_id: data.data?.publish_id || "unknown",
      post_url: null, // TikTok doesn't return URL immediately
    };
  } catch (err) {
    console.error("TikTok publish exception:", err);
    return null;
  }
}

/**
 * Check publish status of a post.
 */
export async function checkPublishStatus(
  publishId: string
): Promise<{ status: string; share_url?: string } | null> {
  const token = process.env.TIKTOK_ACCESS_TOKEN;
  if (!token) return null;

  try {
    const res = await fetch(`${API_BASE}/post/publish/status/fetch/`, {
      method: "POST",
      headers: {
        Authorization: `Bearer ${token}`,
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ publish_id: publishId }),
    });

    const data = await res.json();
    return {
      status: data.data?.status || "unknown",
      share_url: data.data?.share_url,
    };
  } catch {
    return null;
  }
}

/**
 * Generate a video script for TikTok content.
 * Returns a structured script with timing cues.
 */
export function generateVideoScript(params: {
  dogName: string;
  breed: string;
  waitDays: number;
  location: string;
  isUrgent: boolean;
}): {
  hook: string;
  body: string;
  cta: string;
  duration: string;
  suggestedAudio: string;
} {
  const waitText =
    params.waitDays > 365
      ? `${Math.floor(params.waitDays / 365)} years`
      : `${params.waitDays} days`;

  if (params.isUrgent) {
    return {
      hook: `This dog has ${params.waitDays < 3 ? "hours" : "days"} left.`,
      body: `${params.dogName} is a ${params.breed} in ${params.location}. After ${waitText} in a shelter, time is running out. One share could save this life.`,
      cta: "Link in bio. Share this video. Be the reason they survive.",
      duration: "15-30s",
      suggestedAudio: "Emotional piano / sad trending sound",
    };
  }

  return {
    hook: `${waitText} in a shelter. Still waiting.`,
    body: `Meet ${params.dogName}, a ${params.breed} in ${params.location}. While millions of dogs find homes, ${params.dogName} keeps waiting. Every day, they hope today is the day.`,
    cta: "Could you be the one? Link in bio.",
    duration: "30-60s",
    suggestedAudio: "Hopeful / emotional trending sound",
  };
}
