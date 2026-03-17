import { NextResponse } from "next/server";
import { updateUrgencyLevels } from "@/lib/rescuegroups/sync";

export const runtime = "nodejs";

export async function GET(request: Request) {
  const authHeader = request.headers.get("authorization");
  if (authHeader !== `Bearer ${process.env.CRON_SECRET}`) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  try {
    const result = await updateUrgencyLevels();

    return NextResponse.json({
      success: true,
      ...result,
      message: `Updated urgency for ${result.updated} dogs, ${result.errors} errors`,
    });
  } catch (error) {
    console.error("Urgency update error:", error);
    return NextResponse.json(
      { success: false, error: String(error) },
      { status: 500 }
    );
  }
}
