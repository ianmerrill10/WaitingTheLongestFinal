/**
 * Bluesky AT Protocol client.
 *
 * Requires: Bluesky account
 * Auth: App Password
 *
 * Environment variables:
 *   BLUESKY_HANDLE (e.g., "waitingthelongest.bsky.social")
 *   BLUESKY_APP_PASSWORD
 */

const PDS_URL = "https://bsky.social/xrpc";

let cachedSession: {
  accessJwt: string;
  did: string;
  expires: number;
} | null = null;

/**
 * Create a session (login).
 */
async function createSession(): Promise<{
  accessJwt: string;
  did: string;
} | null> {
  if (cachedSession && cachedSession.expires > Date.now()) {
    return cachedSession;
  }

  const handle = process.env.BLUESKY_HANDLE;
  const password = process.env.BLUESKY_APP_PASSWORD;
  if (!handle || !password) {
    console.error("Bluesky: Missing HANDLE or APP_PASSWORD");
    return null;
  }

  try {
    const res = await fetch(`${PDS_URL}/com.atproto.server.createSession`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ identifier: handle, password }),
    });

    const data = await res.json();
    if (!res.ok || !data.accessJwt) {
      console.error("Bluesky auth error:", data);
      return null;
    }

    cachedSession = {
      accessJwt: data.accessJwt,
      did: data.did,
      expires: Date.now() + 55 * 60 * 1000, // ~55 min
    };
    return cachedSession;
  } catch (err) {
    console.error("Bluesky auth exception:", err);
    return null;
  }
}

interface BlueskyPostResult {
  uri: string;
  cid: string;
  post_url: string;
}

/**
 * Create a post on Bluesky.
 */
export async function createPost(params: {
  text: string;
  imageBlob?: { ref: { $link: string }; mimeType: string; size: number };
  link?: { uri: string; title: string; description: string };
}): Promise<BlueskyPostResult | null> {
  const session = await createSession();
  if (!session) return null;

  try {
    const record: Record<string, unknown> = {
      $type: "app.bsky.feed.post",
      text: params.text,
      createdAt: new Date().toISOString(),
    };

    // Add facets for links in text
    const urlRegex = /https?:\/\/[^\s]+/g;
    const facets: Array<{
      index: { byteStart: number; byteEnd: number };
      features: Array<{ $type: string; uri: string }>;
    }> = [];

    let match;
    while ((match = urlRegex.exec(params.text)) !== null) {
      const byteStart = Buffer.byteLength(params.text.slice(0, match.index));
      const byteEnd = byteStart + Buffer.byteLength(match[0]);
      facets.push({
        index: { byteStart, byteEnd },
        features: [
          { $type: "app.bsky.richtext.facet#link", uri: match[0] },
        ],
      });
    }
    if (facets.length > 0) record.facets = facets;

    // Add image embed
    if (params.imageBlob) {
      record.embed = {
        $type: "app.bsky.embed.images",
        images: [{ alt: "Dog photo", image: params.imageBlob }],
      };
    }

    // Add external link embed
    if (params.link && !params.imageBlob) {
      record.embed = {
        $type: "app.bsky.embed.external",
        external: {
          uri: params.link.uri,
          title: params.link.title,
          description: params.link.description,
        },
      };
    }

    const res = await fetch(`${PDS_URL}/com.atproto.repo.createRecord`, {
      method: "POST",
      headers: {
        Authorization: `Bearer ${session.accessJwt}`,
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        repo: session.did,
        collection: "app.bsky.feed.post",
        record,
      }),
    });

    const data = await res.json();
    if (!res.ok || !data.uri) {
      console.error("Bluesky post error:", data);
      return null;
    }

    // Convert AT URI to web URL
    const rkey = data.uri.split("/").pop();
    const handle = process.env.BLUESKY_HANDLE || "";

    return {
      uri: data.uri,
      cid: data.cid,
      post_url: `https://bsky.app/profile/${handle}/post/${rkey}`,
    };
  } catch (err) {
    console.error("Bluesky post exception:", err);
    return null;
  }
}

/**
 * Upload an image blob to Bluesky.
 */
export async function uploadImage(
  imageUrl: string
): Promise<{
  ref: { $link: string };
  mimeType: string;
  size: number;
} | null> {
  const session = await createSession();
  if (!session) return null;

  try {
    const imgRes = await fetch(imageUrl);
    if (!imgRes.ok) return null;

    const buffer = Buffer.from(await imgRes.arrayBuffer());
    const mimeType = imgRes.headers.get("content-type") || "image/jpeg";

    const res = await fetch(`${PDS_URL}/com.atproto.repo.uploadBlob`, {
      method: "POST",
      headers: {
        Authorization: `Bearer ${session.accessJwt}`,
        "Content-Type": mimeType,
      },
      body: buffer,
    });

    const data = await res.json();
    if (!data.blob) return null;

    return data.blob;
  } catch (err) {
    console.error("Bluesky image upload error:", err);
    return null;
  }
}

/**
 * Delete a post.
 */
export async function deletePost(uri: string): Promise<boolean> {
  const session = await createSession();
  if (!session) return false;

  try {
    const rkey = uri.split("/").pop();

    const res = await fetch(`${PDS_URL}/com.atproto.repo.deleteRecord`, {
      method: "POST",
      headers: {
        Authorization: `Bearer ${session.accessJwt}`,
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        repo: session.did,
        collection: "app.bsky.feed.post",
        rkey,
      }),
    });
    return res.ok;
  } catch {
    return false;
  }
}
