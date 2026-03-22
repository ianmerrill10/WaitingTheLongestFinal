"use client";

import { useState, useEffect, useCallback } from "react";
import Link from "next/link";

interface DashboardData {
  dogs: { total: number; critical: number; highUrgency: number; mediumUrgency: number; withEuthanasiaDate: number };
  verification: { total: number; verified: number; unverified: number; notFound: number; pending: number; neverChecked: number; stale: number };
  dateConfidence: { verified: number; high: number; medium: number; low: number; accuracyPct: number };
  dateSources: Record<string, number>;
  dailyTrends: Array<Record<string, unknown>>;
  recentAudits: Array<Record<string, unknown>>;
}

export default function DataDashboard() {
  const [data, setData] = useState<DashboardData | null>(null);
  const [loading, setLoading] = useState(true);

  const fetchData = useCallback(async () => {
    try {
      const res = await fetch("/api/admin/dashboard");
      if (res.ok) setData(await res.json());
    } catch { /* ignore */ }
    setLoading(false);
  }, []);

  useEffect(() => { fetchData(); }, [fetchData]);

  if (loading) {
    return (
      <div className="max-w-7xl mx-auto px-4 py-8">
        <h1 className="text-2xl font-bold mb-6">Data Dashboard</h1>
        <div className="grid grid-cols-4 gap-4 mb-8">
          {[1,2,3,4].map(i => (
            <div key={i} className="bg-white rounded-lg border p-5 animate-pulse">
              <div className="h-4 bg-gray-200 rounded w-20 mb-3" />
              <div className="h-8 bg-gray-200 rounded w-24" />
            </div>
          ))}
        </div>
      </div>
    );
  }

  if (!data) {
    return (
      <div className="max-w-7xl mx-auto px-4 py-8">
        <h1 className="text-2xl font-bold mb-4">Data Dashboard</h1>
        <p className="text-red-600">Failed to load dashboard data.</p>
      </div>
    );
  }

  const v = data.verification;
  const verifiedPct = v.total > 0 ? Math.round((v.verified / v.total) * 1000) / 10 : 0;
  const daysToFull = v.neverChecked > 0 ? Math.round(v.neverChecked / 600 * 10) / 10 : 0;

  return (
    <div className="max-w-7xl mx-auto px-4 py-8">
      <div className="flex items-center justify-between mb-6">
        <div>
          <Link href="/admin" className="text-sm text-gray-500 hover:text-gray-700">&larr; Admin</Link>
          <h1 className="text-2xl font-bold text-gray-900">Data Dashboard</h1>
        </div>
        <button onClick={fetchData} className="px-4 py-2 bg-gray-100 hover:bg-gray-200 rounded-lg text-sm font-medium transition">
          Refresh
        </button>
      </div>

      {/* Overview Cards */}
      <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-8">
        <Card label="Total Dogs" value={data.dogs.total.toLocaleString()} color="text-gray-900" />
        <Card label="Verified" value={`${verifiedPct}%`} sub={`${v.verified.toLocaleString()} of ${v.total.toLocaleString()}`} color={verifiedPct > 80 ? "text-green-600" : verifiedPct > 40 ? "text-amber-600" : "text-red-600"} />
        <Card label="Date Accuracy" value={`${data.dateConfidence.accuracyPct}%`} sub={`${(data.dateConfidence.verified + data.dateConfidence.high).toLocaleString()} verified+high`} color={data.dateConfidence.accuracyPct > 80 ? "text-green-600" : "text-amber-600"} />
        <Card label="Urgent Dogs" value={(data.dogs.critical + data.dogs.highUrgency).toString()} sub={`${data.dogs.critical} critical, ${data.dogs.highUrgency} high`} color={data.dogs.critical > 0 ? "text-red-600" : "text-green-600"} />
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6 mb-8">
        {/* Verification Progress */}
        <div className="bg-white rounded-lg border p-6">
          <h2 className="text-lg font-bold mb-4">Verification Progress</h2>
          <div className="mb-3">
            <div className="flex justify-between text-sm mb-1">
              <span className="text-gray-600">Coverage</span>
              <span className="font-bold text-green-600">{verifiedPct}%</span>
            </div>
            <div className="h-4 bg-gray-100 rounded-full overflow-hidden">
              <div className="h-full bg-green-500 rounded-full transition-all" style={{ width: `${verifiedPct}%` }} />
            </div>
          </div>
          <div className="space-y-2 text-sm">
            <Row label="Verified (confirmed available)" value={v.verified} color="text-green-600" />
            <Row label="Not Found (removed)" value={v.notFound} color="text-amber-600" />
            <Row label="Pending (adoption pending)" value={v.pending} color="text-blue-600" />
            <Row label="Never Checked" value={v.neverChecked} color="text-gray-500" />
            <Row label="Stale (>7 days)" value={v.stale} color="text-gray-400" />
            <div className="pt-2 border-t mt-2">
              <Row label="Est. days to 100%" value={`~${daysToFull}`} color="text-gray-900" />
            </div>
          </div>
        </div>

        {/* Date Confidence */}
        <div className="bg-white rounded-lg border p-6">
          <h2 className="text-lg font-bold mb-4">Date Confidence Distribution</h2>
          <div className="space-y-3">
            <ConfBar label="Verified" count={data.dateConfidence.verified} total={data.dogs.total} color="bg-green-500" />
            <ConfBar label="High" count={data.dateConfidence.high} total={data.dogs.total} color="bg-blue-500" />
            <ConfBar label="Medium" count={data.dateConfidence.medium} total={data.dogs.total} color="bg-amber-400" />
            <ConfBar label="Low" count={data.dateConfidence.low} total={data.dogs.total} color="bg-red-400" />
          </div>

          <h3 className="text-sm font-bold mt-6 mb-3 text-gray-700">Date Sources</h3>
          <div className="space-y-1 text-sm">
            {Object.entries(data.dateSources)
              .filter(([, count]) => count > 0)
              .sort(([, a], [, b]) => b - a)
              .map(([source, count]) => (
                <div key={source} className="flex justify-between">
                  <span className="text-gray-500 text-xs truncate mr-2">{source.replace(/_/g, " ")}</span>
                  <span className="text-gray-900 font-medium text-xs tabular-nums">{count.toLocaleString()}</span>
                </div>
              ))
            }
          </div>
        </div>
      </div>

      {/* Daily Trends */}
      {data.dailyTrends.length > 0 && (
        <div className="bg-white rounded-lg border p-6 mb-8">
          <h2 className="text-lg font-bold mb-4">Daily Trends</h2>
          <div className="overflow-x-auto">
            <table className="w-full text-sm">
              <thead>
                <tr className="border-b">
                  <th className="text-left py-2 text-gray-500 font-medium">Date</th>
                  <th className="text-right py-2 text-gray-500 font-medium">Dogs</th>
                  <th className="text-right py-2 text-gray-500 font-medium">Verified %</th>
                  <th className="text-right py-2 text-gray-500 font-medium">Date Acc %</th>
                  <th className="text-right py-2 text-gray-500 font-medium">Avg Wait</th>
                  <th className="text-right py-2 text-gray-500 font-medium">Added</th>
                  <th className="text-right py-2 text-gray-500 font-medium">Adopted</th>
                </tr>
              </thead>
              <tbody>
                {data.dailyTrends.slice(0, 14).map((row: Record<string, unknown>, i: number) => (
                  <tr key={i} className="border-b border-gray-50 hover:bg-gray-50">
                    <td className="py-2 text-gray-900 font-medium">{String(row.stat_date)}</td>
                    <td className="py-2 text-right tabular-nums">{Number(row.total_available_dogs || 0).toLocaleString()}</td>
                    <td className="py-2 text-right tabular-nums">{Number(row.verification_coverage_pct || 0).toFixed(1)}%</td>
                    <td className="py-2 text-right tabular-nums">{Number(row.date_accuracy_pct || 0).toFixed(1)}%</td>
                    <td className="py-2 text-right tabular-nums">{Number(row.avg_wait_days_high_confidence || row.avg_wait_days_all || 0).toFixed(1)}d</td>
                    <td className="py-2 text-right tabular-nums text-green-600">{Number(row.dogs_added_today || 0)}</td>
                    <td className="py-2 text-right tabular-nums text-blue-600">{Number(row.dogs_adopted_today || 0)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}

      {/* Recent Audits */}
      {data.recentAudits.length > 0 && (
        <div className="bg-white rounded-lg border p-6">
          <h2 className="text-lg font-bold mb-4">Recent Audit Runs</h2>
          <div className="overflow-x-auto">
            <table className="w-full text-sm">
              <thead>
                <tr className="border-b">
                  <th className="text-left py-2 text-gray-500 font-medium">Time</th>
                  <th className="text-left py-2 text-gray-500 font-medium">Mode</th>
                  <th className="text-left py-2 text-gray-500 font-medium">Trigger</th>
                  <th className="text-right py-2 text-gray-500 font-medium">Status</th>
                  <th className="text-right py-2 text-gray-500 font-medium">Duration</th>
                </tr>
              </thead>
              <tbody>
                {data.recentAudits.map((run: Record<string, unknown>, i: number) => (
                  <tr key={i} className="border-b border-gray-50 hover:bg-gray-50">
                    <td className="py-2 text-gray-900">{new Date(String(run.started_at)).toLocaleString()}</td>
                    <td className="py-2">{String(run.run_type || "")}</td>
                    <td className="py-2 text-gray-500">{String(run.trigger_source || "")}</td>
                    <td className="py-2 text-right">
                      <span className={`px-2 py-0.5 rounded text-xs font-medium ${run.status === "completed" ? "bg-green-100 text-green-700" : run.status === "failed" ? "bg-red-100 text-red-700" : "bg-gray-100 text-gray-600"}`}>
                        {String(run.status || "")}
                      </span>
                    </td>
                    <td className="py-2 text-right tabular-nums text-gray-500">{run.duration_ms ? `${(Number(run.duration_ms) / 1000).toFixed(1)}s` : "--"}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}
    </div>
  );
}

function Card({ label, value, sub, color }: { label: string; value: string; sub?: string; color?: string }) {
  return (
    <div className="bg-white rounded-lg border p-5">
      <p className="text-xs text-gray-500 uppercase tracking-wide mb-1">{label}</p>
      <p className={`text-2xl font-bold ${color || "text-gray-900"}`}>{value}</p>
      {sub && <p className="text-xs text-gray-400 mt-1">{sub}</p>}
    </div>
  );
}

function Row({ label, value, color }: { label: string; value: number | string; color?: string }) {
  return (
    <div className="flex justify-between">
      <span className="text-gray-500">{label}</span>
      <span className={`font-medium ${color || "text-gray-900"}`}>{typeof value === "number" ? value.toLocaleString() : value}</span>
    </div>
  );
}

function ConfBar({ label, count, total, color }: { label: string; count: number; total: number; color: string }) {
  const pct = total > 0 ? (count / total) * 100 : 0;
  return (
    <div>
      <div className="flex justify-between text-sm mb-1">
        <span className="text-gray-600">{label}</span>
        <span className="text-gray-900 font-medium">{count.toLocaleString()} ({pct.toFixed(1)}%)</span>
      </div>
      <div className="h-2.5 bg-gray-100 rounded-full overflow-hidden">
        <div className={`h-full ${color} rounded-full`} style={{ width: `${pct}%` }} />
      </div>
    </div>
  );
}
