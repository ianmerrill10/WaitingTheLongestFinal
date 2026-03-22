/**
 * Reddit API client.
 *
 * Requires: Reddit Developer App (script or web type)
 * Auth: OAuth 2.0
 *
 * Environment variables:
 *   REDDIT_CLIENT_ID
 *   REDDIT_CLIENT_SECRET
 *   REDDIT_USERNAME
 *   REDDIT_PASSWORD
 *   REDDIT_USER_AGENT
 */

const API_BASE = "https://oauth.reddit.com";
const AUTH_URL = "https://www.reddit.com/api/v1/access_token";

let cachedToken: { token: string; expires: number } | null = null;

/**
 * Get an OAuth token using password grant.
 */
async function getToken(): Promise<string | null> {
  if (cachedToken && cachedToken.expires > Date.now()) {
    return cachedToken.token;
  }

  const clientId = process.env.REDDIT_CLIENT_ID;
  const clientSecret = process.env.REDDIT_CLIENT_SECRET;
  const username = process.env.REDDIT_USERNAME;
  const password = process.env.REDDIT_PASSWORD;

  if (!clientId || !clientSecret || !username || !password) {
    console.error("Reddit: Missing credentials");
    return null;
  }

  try {
    const auth = Buffer.from(`${clientId}:${clientSecret}`).toString("base64");
    const body = new URLSearchParams({
      grant_type: "password",
      username,
      password,
    });

    const res = await fetch(AUTH_URL, {
      method: "POST",
      headers: {
        Authorization: `Basic ${auth}`,
        "Content-Type": "application/x-www-form-urlencoded",
        "User-Agent": process.env.REDDIT_USER_AGENT || "WaitingTheLongest/1.0",
      },
      body: body.toString(),
    });

    const data = await res.json();
    if (data.access_token) {
      cachedToken = {
        token: data.access_token,
        expires: Date.now() + (data.expires_in - 60) * 1000,
      };
      return data.access_token;
    }
    console.error("Reddit auth error:", data);
    return null;
  } catch (err) {
    console.error("Reddit auth exception:", err);
    return null;
  }
}

interface RedditPostResult {
  id: string;
  post_url: string;
}

/**
 * Submit a link post to a subreddit.
 */
export async function submitLinkPost(params: {
  subreddit: string;
  title: string;
  url: string;
  flairId?: string;
}): Promise<RedditPostResult | null> {
  const token = await getToken();
  if (!token) return null;

  try {
    const body = new URLSearchParams({
      sr: params.subreddit,
      kind: "link",
      title: params.title,
      url: params.url,
      resubmit: "true",
    });

    if (params.flairId) {
      body.append("flair_id", params.flairId);
    }

    const res = await fetch(`${API_BASE}/api/submit`, {
      method: "POST",
      headers: {
        Authorization: `Bearer ${token}`,
        "Content-Type": "application/x-www-form-urlencoded",
        "User-Agent": process.env.REDDIT_USER_AGENT || "WaitingTheLongest/1.0",
      },
      body: body.toString(),
    });

    const data = await res.json();
    if (data.json?.data?.url) {
      return {
        id: data.json.data.id || data.json.data.name,
        post_url: data.json.data.url,
      };
    }
    console.error("Reddit submit error:", data);
    return null;
  } catch (err) {
    console.error("Reddit submit exception:", err);
    return null;
  }
}

/**
 * Submit a text (self) post.
 */
export async function submitTextPost(params: {
  subreddit: string;
  title: string;
  text: string;
  flairId?: string;
}): Promise<RedditPostResult | null> {
  const token = await getToken();
  if (!token) return null;

  try {
    const body = new URLSearchParams({
      sr: params.subreddit,
      kind: "self",
      title: params.title,
      text: params.text,
    });

    if (params.flairId) {
      body.append("flair_id", params.flairId);
    }

    const res = await fetch(`${API_BASE}/api/submit`, {
      method: "POST",
      headers: {
        Authorization: `Bearer ${token}`,
        "Content-Type": "application/x-www-form-urlencoded",
        "User-Agent": process.env.REDDIT_USER_AGENT || "WaitingTheLongest/1.0",
      },
      body: body.toString(),
    });

    const data = await res.json();
    if (data.json?.data?.url) {
      return {
        id: data.json.data.id || data.json.data.name,
        post_url: data.json.data.url,
      };
    }
    return null;
  } catch (err) {
    console.error("Reddit text post exception:", err);
    return null;
  }
}

/**
 * Target subreddits for dog adoption content.
 */
export const TARGET_SUBREDDITS = [
  "AdoptableDogs",
  "rescuedogs",
  "dogs",
  "pitbulls",
  "SeniorDogs",
  "BeforeNAfterAdoption",
];
