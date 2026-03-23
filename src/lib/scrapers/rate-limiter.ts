/**
 * Scraper rate limiter — global + per-domain throttling.
 * Prevents getting blocked by shelter websites.
 */

interface DomainState {
  requests: number[];
  blockedUntil: number;
}

const GLOBAL_MAX_PER_SECOND = 10;
const DOMAIN_MAX_PER_SECOND = 2;
const BLOCK_DURATION_MS = 5 * 60 * 1000; // 5 minutes on 429

const globalRequests: number[] = [];
const domainStates = new Map<string, DomainState>();

function getDomain(url: string): string {
  try {
    return new URL(url).hostname;
  } catch {
    return url;
  }
}

function cleanOldRequests(timestamps: number[], windowMs: number): number[] {
  const cutoff = Date.now() - windowMs;
  return timestamps.filter((t) => t > cutoff);
}

function getDomainState(domain: string): DomainState {
  if (!domainStates.has(domain)) {
    domainStates.set(domain, { requests: [], blockedUntil: 0 });
  }
  return domainStates.get(domain)!;
}

/**
 * Wait until we can make a request to this URL.
 * Enforces both global and per-domain rate limits.
 */
export async function waitForSlot(url: string): Promise<void> {
  const domain = getDomain(url);
  const state = getDomainState(domain);

  // Check if domain is blocked (429 backoff)
  if (state.blockedUntil > Date.now()) {
    const waitMs = state.blockedUntil - Date.now();
    await new Promise((r) => setTimeout(r, waitMs));
  }

  // Global rate limit
  while (true) {
    const now = Date.now();
    const recent = cleanOldRequests(globalRequests, 1000);
    globalRequests.length = 0;
    globalRequests.push(...recent);

    if (recent.length < GLOBAL_MAX_PER_SECOND) break;

    const waitMs = 1000 - (now - recent[0]);
    if (waitMs > 0) await new Promise((r) => setTimeout(r, waitMs));
  }

  // Per-domain rate limit
  while (true) {
    const now = Date.now();
    state.requests = cleanOldRequests(state.requests, 1000);

    if (state.requests.length < DOMAIN_MAX_PER_SECOND) break;

    const waitMs = 1000 - (now - state.requests[0]);
    if (waitMs > 0) await new Promise((r) => setTimeout(r, waitMs));
  }

  // Record this request
  const now = Date.now();
  globalRequests.push(now);
  state.requests.push(now);
}

/**
 * Mark a domain as blocked (received 429).
 */
export function markBlocked(url: string): void {
  const domain = getDomain(url);
  const state = getDomainState(domain);
  state.blockedUntil = Date.now() + BLOCK_DURATION_MS;
}

/**
 * Rate-limited fetch wrapper.
 */
export async function scraperFetch(
  url: string,
  options: RequestInit = {}
): Promise<Response> {
  await waitForSlot(url);

  const headers: Record<string, string> = {
    "User-Agent":
      "WaitingTheLongest.com/1.0 (Dog Rescue Aggregator; +https://waitingthelongest.com)",
    Accept: "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
    ...(options.headers as Record<string, string>),
  };

  const response = await fetch(url, {
    ...options,
    headers,
    signal: AbortSignal.timeout(15000),
  });

  if (response.status === 429) {
    markBlocked(url);
    throw new Error(`Rate limited (429) by ${getDomain(url)}`);
  }

  return response;
}

/**
 * Fetch HTML content from a URL with rate limiting.
 */
export async function fetchHTML(url: string): Promise<string> {
  const response = await scraperFetch(url);
  if (!response.ok) {
    throw new Error(`HTTP ${response.status} from ${url}`);
  }
  return response.text();
}

/**
 * Fetch JSON from a URL with rate limiting.
 */
export async function fetchJSON<T = unknown>(url: string): Promise<T> {
  const response = await scraperFetch(url, {
    headers: {
      Accept: "application/json",
    },
  });
  if (!response.ok) {
    throw new Error(`HTTP ${response.status} from ${url}`);
  }
  return response.json() as Promise<T>;
}
