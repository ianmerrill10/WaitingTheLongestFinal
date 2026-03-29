import { NextRequest, NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";

export const runtime = "nodejs";
export const dynamic = "force-dynamic";

/**
 * GET /api/admin/onboarding — Pipeline overview + list
 * Query params: stage, status, priority, search, page, limit, sort
 */
export async function GET(req: NextRequest) {
  const supabase = createAdminClient();
  const url = req.nextUrl;

  const stage = url.searchParams.get("stage");
  const status = url.searchParams.get("status");
  const priority = url.searchParams.get("priority");
  const search = url.searchParams.get("search");
  const page = parseInt(url.searchParams.get("page") || "1");
  const limit = Math.min(parseInt(url.searchParams.get("limit") || "25"), 100);
  const sort = url.searchParams.get("sort") || "newest";

  try {
    // Pipeline stats — count by stage
    const [
      reviewCount,
      outreachCount,
      agreementCount,
      integrationCount,
      testingCount,
      activeCount,
      closedCount,
      totalCount,
    ] = await Promise.all([
      supabase.from("shelter_onboarding").select("id", { count: "exact", head: true }).eq("stage", "review"),
      supabase.from("shelter_onboarding").select("id", { count: "exact", head: true }).eq("stage", "outreach"),
      supabase.from("shelter_onboarding").select("id", { count: "exact", head: true }).eq("stage", "agreement"),
      supabase.from("shelter_onboarding").select("id", { count: "exact", head: true }).eq("stage", "integration"),
      supabase.from("shelter_onboarding").select("id", { count: "exact", head: true }).eq("stage", "testing"),
      supabase.from("shelter_onboarding").select("id", { count: "exact", head: true }).eq("stage", "active"),
      supabase.from("shelter_onboarding").select("id", { count: "exact", head: true }).eq("stage", "closed"),
      supabase.from("shelter_onboarding").select("id", { count: "exact", head: true }),
    ]);

    const pipeline = {
      review: reviewCount.count || 0,
      outreach: outreachCount.count || 0,
      agreement: agreementCount.count || 0,
      integration: integrationCount.count || 0,
      testing: testingCount.count || 0,
      active: activeCount.count || 0,
      closed: closedCount.count || 0,
      total: totalCount.count || 0,
    };

    // Priority counts
    const [urgentCount, highCount] = await Promise.all([
      supabase.from("shelter_onboarding").select("id", { count: "exact", head: true })
        .eq("priority", "urgent").not("stage", "in", "(active,closed)"),
      supabase.from("shelter_onboarding").select("id", { count: "exact", head: true })
        .eq("priority", "high").not("stage", "in", "(active,closed)"),
    ]);

    // Build list query
    let query = supabase
      .from("shelter_onboarding")
      .select("*");

    if (stage) query = query.eq("stage", stage);
    if (status) query = query.eq("status", status);
    if (priority) query = query.eq("priority", priority);
    if (search) {
      const escaped = search.replace(/[%_\\]/g, "\\$&");
      query = query.or(
        `org_name.ilike.%${escaped}%,contact_email.ilike.%${escaped}%,contact_first_name.ilike.%${escaped}%,contact_last_name.ilike.%${escaped}%`
      );
    }

    // Sort
    switch (sort) {
      case "oldest":
        query = query.order("created_at", { ascending: true });
        break;
      case "priority":
        query = query.order("score", { ascending: false }).order("created_at", { ascending: false });
        break;
      case "name":
        query = query.order("org_name", { ascending: true });
        break;
      default:
        query = query.order("created_at", { ascending: false });
    }

    // Pagination
    const from = (page - 1) * limit;
    query = query.range(from, from + limit - 1);

    const { data: applications, error } = await query;
    if (error) throw error;

    return NextResponse.json({
      pipeline,
      priority_alerts: {
        urgent: urgentCount.count || 0,
        high: highCount.count || 0,
      },
      applications: applications || [],
      pagination: {
        page,
        limit,
        total: pipeline.total,
      },
    });
  } catch (err) {
    console.error("Admin onboarding GET error:", err);
    return NextResponse.json({ error: "Failed to load pipeline" }, { status: 500 });
  }
}

/**
 * PATCH /api/admin/onboarding — Update application (review, approve, reject, advance stage)
 */
export async function PATCH(req: NextRequest) {
  const supabase = createAdminClient();

  try {
    const body = await req.json();
    const { id, action, ...fields } = body as {
      id: string;
      action: "review" | "approve" | "reject" | "advance" | "update";
      [key: string]: unknown;
    };

    if (!id) {
      return NextResponse.json({ error: "Missing application ID" }, { status: 400 });
    }

    const now = new Date().toISOString();
    let updateData: Record<string, unknown> = { updated_at: now };

    switch (action) {
      case "review":
        updateData.status = "under_review";
        updateData.reviewed_at = now;
        if (fields.assigned_to) updateData.assigned_to = fields.assigned_to;
        if (fields.internal_notes) updateData.internal_notes = fields.internal_notes;
        break;

      case "approve":
        updateData.status = "pending_agreement";
        updateData.stage = "agreement";
        updateData.approved_at = now;
        break;

      case "reject":
        updateData.status = "rejected";
        updateData.stage = "closed";
        updateData.rejected_at = now;
        if (fields.rejection_reason) updateData.rejection_reason = fields.rejection_reason;
        break;

      case "advance": {
        const stageOrder = ["review", "outreach", "agreement", "integration", "testing", "active"];
        const currentStageIndex = stageOrder.indexOf(fields.current_stage as string);
        if (currentStageIndex >= 0 && currentStageIndex < stageOrder.length - 1) {
          const nextStage = stageOrder[currentStageIndex + 1];
          updateData.stage = nextStage;
          if (nextStage === "active") {
            updateData.status = "active";
            updateData.activated_at = now;
          }
        }
        break;
      }

      case "update":
        if (fields.priority) updateData.priority = fields.priority;
        if (fields.assigned_to !== undefined) updateData.assigned_to = fields.assigned_to;
        if (fields.internal_notes !== undefined) updateData.internal_notes = fields.internal_notes;
        if (fields.score !== undefined) updateData.score = fields.score;
        break;

      default:
        return NextResponse.json({ error: "Invalid action" }, { status: 400 });
    }

    const { data, error } = await supabase
      .from("shelter_onboarding")
      .update(updateData)
      .eq("id", id)
      .select()
      .single();

    if (error) throw error;

    return NextResponse.json({ success: true, application: data });
  } catch (err) {
    console.error("Admin onboarding PATCH error:", err);
    return NextResponse.json({ error: "Failed to update application" }, { status: 500 });
  }
}
