import type { ShelterApiKey } from "@/types/shelter-crm";

interface RateLimitBucket {
  count: number;
  reset_at: number;
}

// In-memory rate limit store (per-key, per-window)
// In production, this would use Redis for multi-instance support
const minuteBuckets = new Map<string, RateLimitBucket>();
const hourBuckets = new Map<string, RateLimitBucket>();
const dayBuckets = new Map<string, RateLimitBucket>();

// Clean up expired buckets every 5 minutes
setInterval(() => {
  const now = Date.now();
  Array.from(minuteBuckets.entries()).forEach(([k, v]) => {
    if (v.reset_at < now) minuteBuckets.delete(k);
  });
  Array.from(hourBuckets.entries()).forEach(([k, v]) => {
    if (v.reset_at < now) hourBuckets.delete(k);
  });
  Array.from(dayBuckets.entries()).forEach(([k, v]) => {
    if (v.reset_at < now) dayBuckets.delete(k);
  });
}, 5 * 60 * 1000);

interface RateLimitResult {
  allowed: boolean;
  limit: number;
  remaining: number;
  reset: number; // Unix timestamp (seconds)
  window: "minute" | "hour" | "day";
  retryAfter?: number; // Seconds until retry
}

function checkBucket(
  store: Map<string, RateLimitBucket>,
  keyId: string,
  limit: number,
  windowMs: number,
  windowName: "minute" | "hour" | "day"
): RateLimitResult {
  const now = Date.now();
  const bucket = store.get(keyId);

  if (!bucket || bucket.reset_at < now) {
    // New window
    store.set(keyId, { count: 1, reset_at: now + windowMs });
    return {
      allowed: true,
      limit,
      remaining: limit - 1,
      reset: Math.ceil((now + windowMs) / 1000),
      window: windowName,
    };
  }

  if (bucket.count >= limit) {
    const retryAfter = Math.ceil((bucket.reset_at - now) / 1000);
    return {
      allowed: false,
      limit,
      remaining: 0,
      reset: Math.ceil(bucket.reset_at / 1000),
      window: windowName,
      retryAfter,
    };
  }

  bucket.count++;
  return {
    allowed: true,
    limit,
    remaining: limit - bucket.count,
    reset: Math.ceil(bucket.reset_at / 1000),
    window: windowName,
  };
}

/**
 * Check rate limits for an API key across all windows (minute, hour, day).
 * Returns the most restrictive result.
 */
export function checkApiKeyRateLimit(key: ShelterApiKey): RateLimitResult {
  const minuteResult = checkBucket(
    minuteBuckets,
    `${key.id}:min`,
    key.rate_limit_per_minute,
    60 * 1000,
    "minute"
  );

  if (!minuteResult.allowed) return minuteResult;

  const hourResult = checkBucket(
    hourBuckets,
    `${key.id}:hr`,
    key.rate_limit_per_hour,
    60 * 60 * 1000,
    "hour"
  );

  if (!hourResult.allowed) return hourResult;

  const dayResult = checkBucket(
    dayBuckets,
    `${key.id}:day`,
    key.rate_limit_per_day,
    24 * 60 * 60 * 1000,
    "day"
  );

  if (!dayResult.allowed) return dayResult;

  // Return the most restrictive remaining
  const results = [minuteResult, hourResult, dayResult];
  const mostRestrictive = results.reduce((min, r) =>
    r.remaining < min.remaining ? r : min
  );

  return mostRestrictive;
}

/**
 * Get rate limit headers for API response.
 */
export function rateLimitHeaders(result: RateLimitResult): Record<string, string> {
  const headers: Record<string, string> = {
    "X-RateLimit-Limit": result.limit.toString(),
    "X-RateLimit-Remaining": result.remaining.toString(),
    "X-RateLimit-Reset": result.reset.toString(),
    "X-RateLimit-Window": result.window,
  };

  if (result.retryAfter !== undefined) {
    headers["Retry-After"] = result.retryAfter.toString();
  }

  return headers;
}
