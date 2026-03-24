// Audit Logger - writes structured logs to audit_logs table
import { createAdminClient } from "@/lib/supabase/admin";

export type AuditSeverity = "info" | "warning" | "error" | "critical";
export type AuditType =
  | "date_check"
  | "stale_listing"
  | "photo_check"
  | "duplicate_check"
  | "status_check"
  | "shelter_check"
  | "data_quality"
  | "description_date_parse"
  | "age_sanity_check"
  | "description_urgency_parse"
  | "wait_time_reasonableness"
  | "repair"
  | "system";

export interface AuditLogEntry {
  run_id: string;
  audit_type: AuditType;
  entity_type: "dog" | "shelter";
  entity_id?: string;
  severity: AuditSeverity;
  message: string;
  details?: Record<string, unknown>;
  action_taken?: string;
}

export class AuditLogger {
  private supabase = createAdminClient();
  private buffer: AuditLogEntry[] = [];
  private stats = {
    info: 0,
    warning: 0,
    error: 0,
    critical: 0,
    repairs: 0,
  };

  constructor(private runId: string) {}

  log(entry: Omit<AuditLogEntry, "run_id">) {
    this.buffer.push({ ...entry, run_id: this.runId });
    this.stats[entry.severity]++;
    if (entry.action_taken) this.stats.repairs++;
  }

  async flush(): Promise<number> {
    if (this.buffer.length === 0) return 0;
    let written = 0;

    // Write in batches of 100
    for (let i = 0; i < this.buffer.length; i += 100) {
      const batch = this.buffer.slice(i, i + 100);
      const { error } = await this.supabase.from("audit_logs").insert(batch);
      if (!error) written += batch.length;
      else console.error("Audit log flush error:", error.message);
    }

    this.buffer = [];
    return written;
  }

  getStats() {
    return { ...this.stats, buffered: this.buffer.length };
  }

  // Start an audit run — returns the run ID
  static async startRun(
    runType: string,
    triggerSource: string = "cron"
  ): Promise<string> {
    const supabase = createAdminClient();
    const { data, error } = await supabase
      .from("audit_runs")
      .insert({
        run_type: runType,
        status: "running",
        trigger_source: triggerSource,
      })
      .select("id")
      .single();

    if (error || !data) throw new Error(`Failed to start audit run: ${error?.message}`);
    return data.id;
  }

  // Complete an audit run
  static async completeRun(
    runId: string,
    stats: Record<string, unknown>,
    failed = false
  ) {
    const supabase = createAdminClient();
    await supabase
      .from("audit_runs")
      .update({
        status: failed ? "failed" : "completed",
        completed_at: new Date().toISOString(),
        stats,
      })
      .eq("id", runId);
  }

  // Get recent runs
  static async getRecentRuns(limit = 20) {
    const supabase = createAdminClient();
    const { data } = await supabase
      .from("audit_runs")
      .select("*")
      .order("created_at", { ascending: false })
      .limit(limit);
    return data || [];
  }

  // Get logs for a run
  static async getRunLogs(
    runId: string,
    severity?: AuditSeverity,
    limit = 100
  ) {
    const supabase = createAdminClient();
    let query = supabase
      .from("audit_logs")
      .select("*")
      .eq("run_id", runId)
      .order("created_at", { ascending: true })
      .limit(limit);

    if (severity) query = query.eq("severity", severity);
    const { data } = await query;
    return data || [];
  }

  // Get summary counts by severity across recent runs
  static async getSummary(hours = 24) {
    const supabase = createAdminClient();
    const since = new Date(Date.now() - hours * 60 * 60 * 1000).toISOString();

    const results = await Promise.all([
      supabase
        .from("audit_logs")
        .select("id", { count: "exact", head: true })
        .gte("created_at", since)
        .eq("severity", "critical"),
      supabase
        .from("audit_logs")
        .select("id", { count: "exact", head: true })
        .gte("created_at", since)
        .eq("severity", "error"),
      supabase
        .from("audit_logs")
        .select("id", { count: "exact", head: true })
        .gte("created_at", since)
        .eq("severity", "warning"),
      supabase
        .from("audit_logs")
        .select("id", { count: "exact", head: true })
        .gte("created_at", since)
        .not("action_taken", "is", null),
    ]);

    return {
      critical: results[0].count ?? 0,
      error: results[1].count ?? 0,
      warning: results[2].count ?? 0,
      repairs: results[3].count ?? 0,
      period_hours: hours,
    };
  }
}
