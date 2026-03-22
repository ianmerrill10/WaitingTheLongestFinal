/**
 * Facebook / Meta Graph API client.
 *
 * Requires: Meta App, Page Access Token (long-lived)
 * API: Graph API v18+
 * Auth: OAuth 2.0 + Page Token
 *
 * Environment variables:
 *   META_APP_ID
 *   META_APP_SECRET
 *   FACEBOOK_PAGE_ID
 *   FACEBOOK_PAGE_TOKEN
 */

const GRAPH_API = "https://graph.facebook.com/v18.0";

interface FacebookPostResult {
  id: string;
  post_url: string;
}

interface FacebookEngagement {
  likes: number;
  comments: number;
  shares: number;
}

/**
 * Publish a post to a Facebook Page.
 */
export async function publishPost(params: {
  message: string;
  link?: string;
  imageUrl?: string;
}): Promise<FacebookPostResult | null> {
  const pageId = process.env.FACEBOOK_PAGE_ID;
  const token = process.env.FACEBOOK_PAGE_TOKEN;
  if (!pageId || !token) {
    console.error("Facebook: Missing PAGE_ID or PAGE_TOKEN");
    return null;
  }

  try {
    let endpoint = `${GRAPH_API}/${pageId}/feed`;
    const body: Record<string, string> = {
      message: params.message,
      access_token: token,
    };

    if (params.imageUrl) {
      // Photo post
      endpoint = `${GRAPH_API}/${pageId}/photos`;
      body.url = params.imageUrl;
    } else if (params.link) {
      body.link = params.link;
    }

    const res = await fetch(endpoint, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });

    const data = await res.json();
    if (!res.ok) {
      console.error("Facebook publish error:", data);
      return null;
    }

    return {
      id: data.id || data.post_id,
      post_url: `https://www.facebook.com/${data.id || data.post_id}`,
    };
  } catch (err) {
    console.error("Facebook publish exception:", err);
    return null;
  }
}

/**
 * Get engagement metrics for a post.
 */
export async function getPostEngagement(
  postId: string
): Promise<FacebookEngagement | null> {
  const token = process.env.FACEBOOK_PAGE_TOKEN;
  if (!token) return null;

  try {
    const res = await fetch(
      `${GRAPH_API}/${postId}?fields=likes.summary(true),comments.summary(true),shares&access_token=${token}`
    );
    const data = await res.json();

    return {
      likes: data.likes?.summary?.total_count || 0,
      comments: data.comments?.summary?.total_count || 0,
      shares: data.shares?.count || 0,
    };
  } catch {
    return null;
  }
}

/**
 * Delete a post.
 */
export async function deletePost(postId: string): Promise<boolean> {
  const token = process.env.FACEBOOK_PAGE_TOKEN;
  if (!token) return false;

  try {
    const res = await fetch(
      `${GRAPH_API}/${postId}?access_token=${token}`,
      { method: "DELETE" }
    );
    return res.ok;
  } catch {
    return false;
  }
}
