/**
 * X/Twitter API v2 client.
 *
 * Requires: X Developer Account, OAuth 2.0 with PKCE
 * API: X API v2
 *
 * Environment variables:
 *   TWITTER_API_KEY
 *   TWITTER_API_SECRET
 *   TWITTER_ACCESS_TOKEN
 *   TWITTER_ACCESS_SECRET
 *   TWITTER_BEARER_TOKEN
 */

import { createHmac, randomBytes } from "crypto";

const API_BASE = "https://api.twitter.com/2";
const UPLOAD_BASE = "https://upload.twitter.com/1.1";

interface TweetResult {
  id: string;
  post_url: string;
}

/**
 * Generate OAuth 1.0a authorization header.
 */
function generateOAuthHeader(
  method: string,
  url: string,
  params: Record<string, string> = {}
): string {
  const apiKey = process.env.TWITTER_API_KEY || "";
  const apiSecret = process.env.TWITTER_API_SECRET || "";
  const accessToken = process.env.TWITTER_ACCESS_TOKEN || "";
  const accessSecret = process.env.TWITTER_ACCESS_SECRET || "";

  const timestamp = Math.floor(Date.now() / 1000).toString();
  const nonce = randomBytes(16).toString("hex");

  const oauthParams: Record<string, string> = {
    oauth_consumer_key: apiKey,
    oauth_nonce: nonce,
    oauth_signature_method: "HMAC-SHA1",
    oauth_timestamp: timestamp,
    oauth_token: accessToken,
    oauth_version: "1.0",
    ...params,
  };

  const sortedParams = Object.keys(oauthParams)
    .sort()
    .map((k) => `${encodeURIComponent(k)}=${encodeURIComponent(oauthParams[k])}`)
    .join("&");

  const baseString = `${method.toUpperCase()}&${encodeURIComponent(url)}&${encodeURIComponent(sortedParams)}`;
  const signingKey = `${encodeURIComponent(apiSecret)}&${encodeURIComponent(accessSecret)}`;
  const signature = createHmac("sha1", signingKey)
    .update(baseString)
    .digest("base64");

  oauthParams.oauth_signature = signature;

  const headerParams = Object.keys(oauthParams)
    .filter((k) => k.startsWith("oauth_"))
    .sort()
    .map((k) => `${encodeURIComponent(k)}="${encodeURIComponent(oauthParams[k])}"`)
    .join(", ");

  return `OAuth ${headerParams}`;
}

/**
 * Post a tweet.
 */
export async function postTweet(params: {
  text: string;
  mediaIds?: string[];
  replyTo?: string;
}): Promise<TweetResult | null> {
  try {
    const body: Record<string, unknown> = { text: params.text };
    if (params.mediaIds && params.mediaIds.length > 0) {
      body.media = { media_ids: params.mediaIds };
    }
    if (params.replyTo) {
      body.reply = { in_reply_to_tweet_id: params.replyTo };
    }

    const url = `${API_BASE}/tweets`;
    const auth = generateOAuthHeader("POST", url);

    const res = await fetch(url, {
      method: "POST",
      headers: {
        Authorization: auth,
        "Content-Type": "application/json",
      },
      body: JSON.stringify(body),
    });

    const data = await res.json();
    if (!res.ok) {
      console.error("Twitter post error:", data);
      return null;
    }

    const tweetId = data.data?.id;
    return tweetId
      ? {
          id: tweetId,
          post_url: `https://twitter.com/i/status/${tweetId}`,
        }
      : null;
  } catch (err) {
    console.error("Twitter post exception:", err);
    return null;
  }
}

/**
 * Post a thread (array of tweets).
 */
export async function postThread(
  tweets: string[]
): Promise<TweetResult[] | null> {
  const results: TweetResult[] = [];
  let replyTo: string | undefined;

  for (const text of tweets) {
    const result = await postTweet({ text, replyTo });
    if (!result) return null;
    results.push(result);
    replyTo = result.id;
  }

  return results;
}

/**
 * Upload media for use in tweets.
 * Uses v1.1 media upload endpoint.
 */
export async function uploadMedia(
  imageUrl: string
): Promise<string | null> {
  try {
    // Download the image first
    const imgRes = await fetch(imageUrl);
    if (!imgRes.ok) return null;

    const buffer = Buffer.from(await imgRes.arrayBuffer());
    const base64 = buffer.toString("base64");

    const url = `${UPLOAD_BASE}/media/upload.json`;
    const auth = generateOAuthHeader("POST", url);

    const formData = new URLSearchParams();
    formData.append("media_data", base64);

    const res = await fetch(url, {
      method: "POST",
      headers: {
        Authorization: auth,
        "Content-Type": "application/x-www-form-urlencoded",
      },
      body: formData.toString(),
    });

    const data = await res.json();
    return data.media_id_string || null;
  } catch (err) {
    console.error("Twitter media upload error:", err);
    return null;
  }
}

/**
 * Delete a tweet.
 */
export async function deleteTweet(tweetId: string): Promise<boolean> {
  try {
    const url = `${API_BASE}/tweets/${tweetId}`;
    const auth = generateOAuthHeader("DELETE", url);

    const res = await fetch(url, {
      method: "DELETE",
      headers: { Authorization: auth },
    });
    return res.ok;
  } catch {
    return false;
  }
}
