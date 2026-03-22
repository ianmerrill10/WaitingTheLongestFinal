/**
 * Instagram Graph API client (via Meta).
 *
 * Requires: Meta App with Instagram Graph API enabled
 * API: Instagram Graph API
 * Auth: OAuth 2.0 via Meta
 *
 * Environment variables:
 *   INSTAGRAM_ACCOUNT_ID
 *   INSTAGRAM_ACCESS_TOKEN (long-lived page token from Meta)
 */

const GRAPH_API = "https://graph.facebook.com/v18.0";

interface InstagramPostResult {
  id: string;
  post_url: string;
}

/**
 * Publish a single image post to Instagram.
 * Two-step process: create media container, then publish.
 */
export async function publishPost(params: {
  imageUrl: string;
  caption: string;
}): Promise<InstagramPostResult | null> {
  const accountId = process.env.INSTAGRAM_ACCOUNT_ID;
  const token = process.env.INSTAGRAM_ACCESS_TOKEN;
  if (!accountId || !token) {
    console.error("Instagram: Missing ACCOUNT_ID or ACCESS_TOKEN");
    return null;
  }

  try {
    // Step 1: Create media container
    const containerRes = await fetch(
      `${GRAPH_API}/${accountId}/media`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          image_url: params.imageUrl,
          caption: params.caption,
          access_token: token,
        }),
      }
    );
    const containerData = await containerRes.json();
    if (!containerRes.ok || !containerData.id) {
      console.error("Instagram container error:", containerData);
      return null;
    }

    // Step 2: Publish
    const publishRes = await fetch(
      `${GRAPH_API}/${accountId}/media_publish`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          creation_id: containerData.id,
          access_token: token,
        }),
      }
    );
    const publishData = await publishRes.json();
    if (!publishRes.ok || !publishData.id) {
      console.error("Instagram publish error:", publishData);
      return null;
    }

    return {
      id: publishData.id,
      post_url: `https://www.instagram.com/p/${publishData.id}/`,
    };
  } catch (err) {
    console.error("Instagram publish exception:", err);
    return null;
  }
}

/**
 * Publish a carousel post (multiple images).
 */
export async function publishCarousel(params: {
  imageUrls: string[];
  caption: string;
}): Promise<InstagramPostResult | null> {
  const accountId = process.env.INSTAGRAM_ACCOUNT_ID;
  const token = process.env.INSTAGRAM_ACCESS_TOKEN;
  if (!accountId || !token) return null;

  try {
    // Step 1: Create child containers for each image
    const childIds: string[] = [];
    for (const url of params.imageUrls.slice(0, 10)) {
      const res = await fetch(`${GRAPH_API}/${accountId}/media`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          image_url: url,
          is_carousel_item: true,
          access_token: token,
        }),
      });
      const data = await res.json();
      if (data.id) childIds.push(data.id);
    }

    if (childIds.length === 0) return null;

    // Step 2: Create carousel container
    const containerRes = await fetch(
      `${GRAPH_API}/${accountId}/media`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          media_type: "CAROUSEL",
          children: childIds,
          caption: params.caption,
          access_token: token,
        }),
      }
    );
    const containerData = await containerRes.json();
    if (!containerData.id) return null;

    // Step 3: Publish
    const publishRes = await fetch(
      `${GRAPH_API}/${accountId}/media_publish`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          creation_id: containerData.id,
          access_token: token,
        }),
      }
    );
    const publishData = await publishRes.json();
    if (!publishData.id) return null;

    return {
      id: publishData.id,
      post_url: `https://www.instagram.com/p/${publishData.id}/`,
    };
  } catch (err) {
    console.error("Instagram carousel exception:", err);
    return null;
  }
}

/**
 * Get engagement metrics for a post.
 */
export async function getPostInsights(postId: string): Promise<{
  likes: number;
  comments: number;
  saves: number;
  reach: number;
} | null> {
  const token = process.env.INSTAGRAM_ACCESS_TOKEN;
  if (!token) return null;

  try {
    const res = await fetch(
      `${GRAPH_API}/${postId}?fields=like_count,comments_count,insights.metric(saved,reach)&access_token=${token}`
    );
    const data = await res.json();

    const insights = data.insights?.data || [];
    const saved = insights.find((i: { name: string }) => i.name === "saved")?.values?.[0]?.value || 0;
    const reach = insights.find((i: { name: string }) => i.name === "reach")?.values?.[0]?.value || 0;

    return {
      likes: data.like_count || 0,
      comments: data.comments_count || 0,
      saves: saved,
      reach,
    };
  } catch {
    return null;
  }
}
