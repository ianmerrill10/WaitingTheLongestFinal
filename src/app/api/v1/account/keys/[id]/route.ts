import { NextResponse } from "next/server";
import {
  authenticateApiKey,
  authError,
  hasScope,
  scopeError,
} from "@/lib/shelter-api/auth";
import { revokeKey, rotateKey } from "@/lib/shelter-crm/api-keys";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

/**
 * DELETE /api/v1/account/keys/:id — Revoke key
 */
export async function DELETE(
  request: Request,
  { params }: { params: { id: string } }
) {
  const auth = await authenticateApiKey(request);
  if (!auth.authenticated) return authError(auth);
  if (!hasScope(auth.key!, "write")) return scopeError("write");

  // Prevent revoking own key
  if (params.id === auth.key!.id) {
    return NextResponse.json(
      { error: "Cannot revoke the key currently in use" },
      { status: 400 }
    );
  }

  try {
    await revokeKey(params.id, `api_key:${auth.key!.key_prefix}`, "Revoked via API");
    return NextResponse.json({ message: "API key revoked" });
  } catch (err) {
    return NextResponse.json(
      { error: err instanceof Error ? err.message : "Failed to revoke key" },
      { status: 500 }
    );
  }
}

/**
 * POST /api/v1/account/keys/:id/rotate — Rotate key (creates new, old has 24h grace)
 */
export async function POST(
  request: Request,
  { params }: { params: { id: string } }
) {
  const auth = await authenticateApiKey(request);
  if (!auth.authenticated) return authError(auth);
  if (!hasScope(auth.key!, "write")) return scopeError("write");

  try {
    const result = await rotateKey(
      params.id,
      auth.shelter_id!,
      `api_key:${auth.key!.key_prefix}`
    );

    return NextResponse.json({
      data: {
        id: result.key.id,
        key_prefix: result.key.key_prefix,
        label: result.key.label,
      },
      api_key: result.raw_key,
      message: "Key rotated. Old key remains valid for 24 hours. Save the new key now.",
    }, { status: 201 });
  } catch (err) {
    return NextResponse.json(
      { error: err instanceof Error ? err.message : "Failed to rotate key" },
      { status: 500 }
    );
  }
}
