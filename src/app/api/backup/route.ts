// Backup API — exports database tables as JSON for backup purposes
// GET: Export all tables (dogs, shelters, states, breeds, audit_logs, audit_runs)
// Designed to be called by a weekly cron or external backup script

import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";

export const runtime = "nodejs";
export const maxDuration = 300;

export async function GET(request: Request) {
  const authHeader = request.headers.get("authorization");
  const cronSecret = process.env.CRON_SECRET;
  if (!cronSecret || authHeader !== `Bearer ${cronSecret}`) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  const url = new URL(request.url);
  const table = url.searchParams.get("table");
  const format = url.searchParams.get("format") || "json";
  const page = parseInt(url.searchParams.get("page") || "1");
  const limit = Math.min(parseInt(url.searchParams.get("limit") || "5000"), 10000);
  const offset = (page - 1) * limit;

  const supabase = createAdminClient();
  const validTables = [
    "dogs",
    "shelters",
    "states",
    "breeds",
    "audit_logs",
    "audit_runs",
  ];

  try {
    // If no table specified, return a manifest of all tables with counts
    if (!table) {
      const counts: Record<string, number> = {};
      for (const t of validTables) {
        const { count } = await supabase
          .from(t)
          .select("id", { count: "exact", head: true });
        counts[t] = count ?? 0;
      }

      return NextResponse.json({
        backup_manifest: {
          timestamp: new Date().toISOString(),
          tables: counts,
          total_records: Object.values(counts).reduce((a, b) => a + b, 0),
          instructions:
            "Use ?table=dogs&page=1&limit=5000 to export each table page by page",
        },
      });
    }

    if (!validTables.includes(table)) {
      return NextResponse.json(
        { error: `Invalid table. Valid: ${validTables.join(", ")}` },
        { status: 400 }
      );
    }

    // Get total count
    const { count: totalCount } = await supabase
      .from(table)
      .select("id", { count: "exact", head: true });

    // Fetch page of data
    const { data, error } = await supabase
      .from(table)
      .select("*")
      .range(offset, offset + limit - 1)
      .order("created_at", { ascending: true });

    if (error) {
      return NextResponse.json({ error: error.message }, { status: 500 });
    }

    const totalPages = Math.ceil((totalCount ?? 0) / limit);

    if (format === "csv" && data && data.length > 0) {
      // Simple CSV conversion
      const headers = Object.keys(data[0]);
      const csvRows = [headers.join(",")];
      for (const row of data) {
        csvRows.push(
          headers
            .map((h) => {
              const val = (row as Record<string, unknown>)[h];
              if (val === null || val === undefined) return "";
              const str = typeof val === "object" ? JSON.stringify(val) : String(val);
              return str.includes(",") || str.includes('"') || str.includes("\n")
                ? `"${str.replace(/"/g, '""')}"`
                : str;
            })
            .join(",")
        );
      }

      return new Response(csvRows.join("\n"), {
        headers: {
          "Content-Type": "text/csv",
          "Content-Disposition": `attachment; filename="${table}_page${page}.csv"`,
        },
      });
    }

    return NextResponse.json({
      table,
      page,
      limit,
      total_records: totalCount ?? 0,
      total_pages: totalPages,
      has_more: page < totalPages,
      next_page: page < totalPages ? page + 1 : null,
      data,
    });
  } catch (err) {
    return NextResponse.json(
      { error: err instanceof Error ? err.message : String(err) },
      { status: 500 }
    );
  }
}
