import { createAdminClient } from "@/lib/supabase/admin";
import { createHash, randomBytes } from "crypto";
import type {
  ShelterApiKey,
  GenerateApiKeyInput,
  GeneratedApiKey,
  ApiKeyUsageStats,
} from "@/types/shelter-crm";

const supabase = createAdminClient();

const KEY_PREFIX = "wtl_sk_";

function hashKey(rawKey: string): string {
  return createHash("sha256").update(rawKey).digest("hex");
}

/**
 * Generate a new API key for a shelter.
 * Returns the raw key ONCE — it can never be retrieved again.
 */
export async function generateApiKey(
  input: GenerateApiKeyInput
): Promise<GeneratedApiKey> {
  // Generate cryptographically random key
  const randomPart = randomBytes(20).toString("hex"); // 40 hex chars
  const rawKey = `${KEY_PREFIX}${randomPart}`;
  const keyPrefix = rawKey.substring(0, KEY_PREFIX.length + 6); // "wtl_sk_" + 6 chars
  const keyHash = hashKey(rawKey);

  const { data, error } = await supabase
    .from("shelter_api_keys")
    .insert({
      shelter_id: input.shelter_id,
      key_prefix: keyPrefix,
      key_hash: keyHash,
      label: input.label || "Default",
      environment: input.environment || "production",
      scopes: input.scopes || ["read", "write"],
      rate_limit_per_minute: input.rate_limit_per_minute || 60,
      rate_limit_per_hour: input.rate_limit_per_hour || 1000,
      rate_limit_per_day: input.rate_limit_per_day || 10000,
      expires_at: input.expires_at || null,
      created_by: input.created_by || "system",
    })
    .select()
    .single();

  if (error) throw new Error(`Failed to generate API key: ${error.message}`);

  return {
    key: data as ShelterApiKey,
    raw_key: rawKey,
  };
}

/**
 * Validate an API key and return the associated shelter + key record.
 * Updates last_used_at and usage counters.
 */
export async function validateApiKey(
  rawKey: string,
  ip?: string
): Promise<{ valid: boolean; key?: ShelterApiKey; error?: string }> {
  if (!rawKey.startsWith(KEY_PREFIX)) {
    return { valid: false, error: "Invalid key format" };
  }

  const keyHash = hashKey(rawKey);

  const { data, error } = await supabase
    .from("shelter_api_keys")
    .select("*")
    .eq("key_hash", keyHash)
    .eq("is_active", true)
    .single();

  if (error || !data) {
    return { valid: false, error: "Invalid or inactive API key" };
  }

  const key = data as ShelterApiKey;

  // Check expiration
  if (key.expires_at && new Date(key.expires_at) < new Date()) {
    return { valid: false, error: "API key has expired" };
  }

  // Check if revoked
  if (key.revoked_at) {
    return { valid: false, error: "API key has been revoked" };
  }

  // Update usage stats
  await supabase
    .from("shelter_api_keys")
    .update({
      last_used_at: new Date().toISOString(),
      last_used_ip: ip || null,
      usage_count_today: key.usage_count_today + 1,
      usage_count_total: key.usage_count_total + 1,
    })
    .eq("id", key.id);

  return { valid: true, key };
}

/**
 * Revoke an API key immediately.
 */
export async function revokeKey(
  keyId: string,
  revokedBy: string,
  reason?: string
): Promise<void> {
  const { error } = await supabase
    .from("shelter_api_keys")
    .update({
      is_active: false,
      revoked_at: new Date().toISOString(),
      revoked_by: revokedBy,
      revoke_reason: reason || null,
    })
    .eq("id", keyId);

  if (error) throw new Error(`Failed to revoke key: ${error.message}`);
}

/**
 * Rotate a key: create a new one and revoke the old one.
 * Old key remains active for 24h grace period.
 */
export async function rotateKey(
  oldKeyId: string,
  shelterId: string,
  rotatedBy: string
): Promise<GeneratedApiKey> {
  // Get old key details
  const { data: oldKey } = await supabase
    .from("shelter_api_keys")
    .select("*")
    .eq("id", oldKeyId)
    .single();

  if (!oldKey) throw new Error("Key not found");

  // Generate new key with same settings
  const newKey = await generateApiKey({
    shelter_id: shelterId,
    label: oldKey.label,
    environment: oldKey.environment,
    scopes: oldKey.scopes,
    rate_limit_per_minute: oldKey.rate_limit_per_minute,
    rate_limit_per_hour: oldKey.rate_limit_per_hour,
    rate_limit_per_day: oldKey.rate_limit_per_day,
    created_by: rotatedBy,
  });

  // Link new key to old
  await supabase
    .from("shelter_api_keys")
    .update({ rotated_from_key_id: oldKeyId })
    .eq("id", newKey.key.id);

  // Schedule old key revocation (24h grace period)
  const graceExpiry = new Date();
  graceExpiry.setHours(graceExpiry.getHours() + 24);

  await supabase
    .from("shelter_api_keys")
    .update({
      expires_at: graceExpiry.toISOString(),
      revoke_reason: `Rotated — replaced by ${newKey.key.key_prefix}. Grace period until ${graceExpiry.toISOString()}`,
    })
    .eq("id", oldKeyId);

  return newKey;
}

/**
 * Get all API keys for a shelter (never returns full key hash).
 */
export async function getKeysByShelter(
  shelterId: string,
  activeOnly = true
): Promise<ShelterApiKey[]> {
  let query = supabase
    .from("shelter_api_keys")
    .select("*")
    .eq("shelter_id", shelterId)
    .order("created_at", { ascending: false });

  if (activeOnly) {
    query = query.eq("is_active", true);
  }

  const { data, error } = await query;
  if (error) throw new Error(`Failed to get keys: ${error.message}`);
  return (data || []) as ShelterApiKey[];
}

/**
 * Get usage stats for a specific key.
 */
export async function getKeyUsage(keyId: string): Promise<ApiKeyUsageStats | null> {
  const { data: key } = await supabase
    .from("shelter_api_keys")
    .select("id, key_prefix, label, usage_count_today, usage_count_total, last_used_at")
    .eq("id", keyId)
    .single();

  if (!key) return null;

  // Count requests in last hour and last minute from api_request_logs
  const oneHourAgo = new Date(Date.now() - 60 * 60 * 1000).toISOString();
  const oneMinuteAgo = new Date(Date.now() - 60 * 1000).toISOString();

  const [hourResult, minuteResult] = await Promise.all([
    supabase
      .from("api_request_logs")
      .select("*", { count: "exact", head: true })
      .eq("api_key_id", keyId)
      .gte("created_at", oneHourAgo),
    supabase
      .from("api_request_logs")
      .select("*", { count: "exact", head: true })
      .eq("api_key_id", keyId)
      .gte("created_at", oneMinuteAgo),
  ]);

  return {
    key_id: key.id,
    key_prefix: key.key_prefix,
    label: key.label,
    today: key.usage_count_today,
    this_hour: hourResult.count || 0,
    this_minute: minuteResult.count || 0,
    total: key.usage_count_total,
    last_used_at: key.last_used_at,
  };
}

/**
 * Reset daily usage counters (should be called by daily cron).
 */
export async function resetDailyUsageCounters(): Promise<number> {
  const { data, error } = await supabase
    .from("shelter_api_keys")
    .update({ usage_count_today: 0 })
    .gt("usage_count_today", 0)
    .select("id");

  if (error) throw new Error(`Failed to reset counters: ${error.message}`);
  return data?.length || 0;
}

/**
 * Check if a key's rate limit is exceeded.
 */
export async function checkRateLimit(
  key: ShelterApiKey
): Promise<{ allowed: boolean; limit: number; remaining: number; reset: string }> {
  const oneMinuteAgo = new Date(Date.now() - 60 * 1000).toISOString();

  const { count } = await supabase
    .from("api_request_logs")
    .select("*", { count: "exact", head: true })
    .eq("api_key_id", key.id)
    .gte("created_at", oneMinuteAgo);

  const used = count || 0;
  const remaining = Math.max(0, key.rate_limit_per_minute - used);
  const resetTime = new Date(Date.now() + 60 * 1000).toISOString();

  return {
    allowed: used < key.rate_limit_per_minute,
    limit: key.rate_limit_per_minute,
    remaining,
    reset: resetTime,
  };
}
