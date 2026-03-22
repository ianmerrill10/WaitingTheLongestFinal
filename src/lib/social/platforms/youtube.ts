/**
 * YouTube Data API v3 client.
 *
 * Requires: Google Cloud project with YouTube Data API v3 enabled
 * Auth: OAuth 2.0 + API Key
 *
 * Environment variables:
 *   YOUTUBE_API_KEY
 *   YOUTUBE_CHANNEL_ID
 *   YOUTUBE_ACCESS_TOKEN
 *   YOUTUBE_REFRESH_TOKEN
 *   GOOGLE_CLIENT_ID
 *   GOOGLE_CLIENT_SECRET
 */

const API_BASE = "https://www.googleapis.com/youtube/v3";

interface YouTubeVideoResult {
  id: string;
  post_url: string;
}

/**
 * Upload a video to YouTube.
 * Note: Video upload requires resumable upload protocol.
 * This is a simplified version — production would use resumable uploads.
 */
export async function uploadVideo(params: {
  title: string;
  description: string;
  tags: string[];
  categoryId?: string;
  privacyStatus?: "public" | "unlisted" | "private";
  videoBuffer: Buffer;
}): Promise<YouTubeVideoResult | null> {
  const token = process.env.YOUTUBE_ACCESS_TOKEN;
  if (!token) {
    console.error("YouTube: Missing ACCESS_TOKEN");
    return null;
  }

  try {
    // Step 1: Initialize resumable upload
    const initRes = await fetch(
      `https://www.googleapis.com/upload/youtube/v3/videos?uploadType=resumable&part=snippet,status`,
      {
        method: "POST",
        headers: {
          Authorization: `Bearer ${token}`,
          "Content-Type": "application/json",
          "X-Upload-Content-Type": "video/*",
          "X-Upload-Content-Length": params.videoBuffer.length.toString(),
        },
        body: JSON.stringify({
          snippet: {
            title: params.title,
            description: params.description,
            tags: params.tags,
            categoryId: params.categoryId || "15", // Pets & Animals
          },
          status: {
            privacyStatus: params.privacyStatus || "public",
            selfDeclaredMadeForKids: false,
          },
        }),
      }
    );

    const uploadUrl = initRes.headers.get("Location");
    if (!uploadUrl) {
      console.error("YouTube: No upload URL returned");
      return null;
    }

    // Step 2: Upload video data
    const uploadRes = await fetch(uploadUrl, {
      method: "PUT",
      headers: {
        "Content-Type": "video/*",
        "Content-Length": params.videoBuffer.length.toString(),
      },
      body: new Uint8Array(params.videoBuffer) as unknown as BodyInit,
    });

    const data = await uploadRes.json();
    if (!uploadRes.ok || !data.id) {
      console.error("YouTube upload error:", data);
      return null;
    }

    return {
      id: data.id,
      post_url: `https://www.youtube.com/watch?v=${data.id}`,
    };
  } catch (err) {
    console.error("YouTube upload exception:", err);
    return null;
  }
}

/**
 * Get video analytics.
 */
export async function getVideoStats(
  videoId: string
): Promise<{
  views: number;
  likes: number;
  comments: number;
} | null> {
  const apiKey = process.env.YOUTUBE_API_KEY;
  if (!apiKey) return null;

  try {
    const res = await fetch(
      `${API_BASE}/videos?part=statistics&id=${videoId}&key=${apiKey}`
    );
    const data = await res.json();
    const stats = data.items?.[0]?.statistics;

    return stats
      ? {
          views: parseInt(stats.viewCount) || 0,
          likes: parseInt(stats.likeCount) || 0,
          comments: parseInt(stats.commentCount) || 0,
        }
      : null;
  } catch {
    return null;
  }
}

/**
 * Generate a YouTube video description.
 */
export function generateDescription(params: {
  dogName: string;
  breed: string;
  location: string;
  waitDays: number;
  dogUrl: string;
  shelterName: string;
}): string {
  const waitText =
    params.waitDays > 365
      ? `${Math.floor(params.waitDays / 365)} years`
      : `${params.waitDays} days`;

  return `${params.dogName} has been waiting ${waitText} at ${params.shelterName} in ${params.location}.

Meet ${params.dogName}: ${params.dogUrl}

Every day, thousands of dogs sit in shelters waiting for someone to give them a second chance. ${params.dogName} is one of them. Please share this video — you might be the connection that changes everything.

---
Find dogs waiting the longest: https://waitingthelongest.com
Foster a dog: https://waitingthelongest.com/foster
Browse by state: https://waitingthelongest.com/states

#AdoptDontShop #ShelterDog #RescueDog #${params.breed.replace(/\s/g, "")} #WaitingTheLongest`;
}
