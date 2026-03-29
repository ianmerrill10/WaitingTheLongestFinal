"use client";

import { useState, useEffect, useCallback } from "react";
import Link from "next/link";
import SortableTable, { type Column } from "@/components/admin/SortableTable";

// eslint-disable-next-line @typescript-eslint/no-explicit-any
type AnyRow = Record<string, any>;

interface DashboardData {
  generatedAt: string;
  dogs: {
    total: number; critical: number; highUrgency: number; mediumUrgency: number;
    withEuthanasiaDate: number; expiredStillAvailable: number; outcomeUnknown: number;
  };
  verification: {
    total: number; verified: number; unverified: number; notFound: number;
    pending: number; neverChecked: number; stale: number;
  };
  dateConfidence: { verified: number; high: number; medium: number; low: number; accuracyPct: number };
  dateSources: Record<string, number>;
  sourceBreakdown: { rescuegroups: number; scraper: number; other: number };
  stateBreakdown: Array<{ state: string; totalDogs: number; urgentDogs: number; shelterCount: number; urgentPct: number }>;
  topShelters: Array<{
    id: string; name: string; city: string; state: string; dogCount: number;
    urgentCount: number; platform: string | null; website: string | null;
    lastScraped: string | null; orgType: string | null;
  }>;
  dailyTrends: AnyRow[];
  recentAudits: AnyRow[];
}

const TABS = [
  { key: "overview", label: "Overview" },
  { key: "states", label: "By State" },
  { key: "shelters", label: "Shelters" },
  { key: "trends", label: "Daily Trends" },
  { key: "audits", label: "Audits" },
] as const;

type TabKey = (typeof TABS)[number]["key"];

const REFRESH_OPTIONS = [
  { label: "Off", value: 0 },
  { label: "30s", value: 30 },
  { label: "60s", value: 60 },
  { label: "5m", value: 300 },
] as const;

export default function DataDashboard() {
  const [data, setData] = useState<DashboardData | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [tab, setTab] = useState<TabKey>("overview");
  const [autoRefresh, setAutoRefresh] = useState(0);
  const [lastUpdated, setLastUpdated] = useState<Date | null>(null);

  const fetchData = useCallback(async () => {
    try {
      const res = await fetch("/api/admin/dashboard");
      if (res.ok) {
        setData(await res.json());
        setLastUpdated(new Date());
        setError(null);
      } else {
        setError(`Dashboard API returned ${res.status}`);
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed to fetch dashboard data");
    }
    setLoading(false);
  }, []);

  useEffect(() => { fetchData(); }, [fetchData]);

  // Auto-refresh
  useEffect(() => {
    if (autoRefresh <= 0) return;
    const id = setInterval(fetchData, autoRefresh * 1000);
    return () => clearInterval(id);
  }, [autoRefresh, fetchData]);

  // Keyboard shortcut: R to refresh
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if (e.key === "r" && !e.ctrlKey && !e.metaKey && !(e.target instanceof HTMLInputElement)) {
        fetchData();
      }
    };
    window.addEventListener("keydown", handler);
    return () => window.removeEventListener("keydown", handler);
  }, [fetchData]);

  if (loading) {
    return (
      <div className="max-w-7xl mx-auto px-4 py-8">
        <h1 className="text-2xl font-bold mb-6">Data Dashboard</h1>
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-8">
          {[1, 2, 3, 4].map((i) => (
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
        <p className="text-red-600">{error || "Failed to load dashboard data."}</p>
        <button onClick={fetchData} className="mt-4 px-4 py-2 bg-gray-100 hover:bg-gray-200 rounded-lg text-sm">Retry</button>
      </div>
    );
  }

  const v = data.verification;
  const verifiedPct = v.total > 0 ? Math.round((v.verified / v.total) * 1000) / 10 : 0;

  return (
    <div className="max-w-7xl mx-auto px-4 py-8">
      {/* Header */}
      <div className="flex items-center justify-between mb-6">
        <div>
          <Link href="/admin" className="text-sm text-gray-500 hover:text-gray-700">&larr; Admin</Link>
          <h1 className="text-2xl font-bold text-gray-900">Data Dashboard</h1>
        </div>
        <div className="flex items-center gap-3">
          {/* Auto-refresh */}
          <div className="flex items-center gap-1 text-xs">
            <span className="text-gray-400">Auto:</span>
            {REFRESH_OPTIONS.map((opt) => (
              <button
                key={opt.value}
                onClick={() => setAutoRefresh(opt.value)}
                className={`px-2 py-1 rounded ${
                  autoRefresh === opt.value
                    ? "bg-green-100 text-green-700 font-medium"
                    : "text-gray-500 hover:bg-gray-100"
                }`}
              >
                {opt.label}
              </button>
            ))}
          </div>
          <button
            onClick={fetchData}
            className="px-4 py-2 bg-gray-100 hover:bg-gray-200 rounded-lg text-sm font-medium transition"
          >
            Refresh
          </button>
        </div>
      </div>

      {/* Last updated */}
      {lastUpdated && (
        <p className="text-xs text-gray-400 mb-4">
          Last updated: {lastUpdated.toLocaleTimeString()} &middot; Press R to refresh
        </p>
      )}

      {/* Overview cards — always visible */}
      <div className="grid grid-cols-2 md:grid-cols-4 lg:grid-cols-6 gap-4 mb-6">
        <Card label="Total Dogs" value={data.dogs.total.toLocaleString()} />
        <Card
          label="Verified"
          value={`${verifiedPct}%`}
          sub={`${v.verified.toLocaleString()} of ${v.total.toLocaleString()}`}
          color={verifiedPct > 80 ? "text-green-600" : verifiedPct > 40 ? "text-amber-600" : "text-red-600"}
        />
        <Card
          label="Date Accuracy"
          value={`${data.dateConfidence.accuracyPct}%`}
          sub={`${(data.dateConfidence.verified + data.dateConfidence.high).toLocaleString()} verified+high`}
          color={data.dateConfidence.accuracyPct > 80 ? "text-green-600" : "text-amber-600"}
        />
        <Card
          label="Urgent (active)"
          value={(data.dogs.critical + data.dogs.highUrgency).toString()}
          sub={`${data.dogs.critical} critical, ${data.dogs.highUrgency} high`}
          color={data.dogs.critical > 0 ? "text-red-600" : "text-green-600"}
        />
        <Card
          label="RG / Scraper"
          value={`${((data.sourceBreakdown.rescuegroups / Math.max(data.dogs.total, 1)) * 100).toFixed(0)}% / ${((data.sourceBreakdown.scraper / Math.max(data.dogs.total, 1)) * 100).toFixed(0)}%`}
          sub={`${data.sourceBreakdown.rescuegroups.toLocaleString()} + ${data.sourceBreakdown.scraper.toLocaleString()}`}
          color="text-blue-600"
        />
        <Card
          label="Data Health"
          value={data.dogs.expiredStillAvailable > 0 ? "Needs Attention" : "Clean"}
          sub={`${data.dogs.expiredStillAvailable} expired, ${data.dogs.outcomeUnknown} unknown`}
          color={data.dogs.expiredStillAvailable > 0 ? "text-amber-600" : "text-green-600"}
        />
      </div>

      {/* Tabs */}
      <div className="flex gap-1 mb-6 border-b border-gray-200">
        {TABS.map((t) => (
          <button
            key={t.key}
            onClick={() => setTab(t.key)}
            className={`px-4 py-2.5 text-sm font-medium border-b-2 transition ${
              tab === t.key
                ? "border-green-600 text-green-700"
                : "border-transparent text-gray-500 hover:text-gray-700"
            }`}
          >
            {t.label}
          </button>
        ))}
      </div>

      {/* Tab Content */}
      {tab === "overview" && <OverviewTab data={data} verifiedPct={verifiedPct} />}
      {tab === "states" && <StatesTab data={data} />}
      {tab === "shelters" && <SheltersTab data={data} />}
      {tab === "trends" && <TrendsTab data={data} />}
      {tab === "audits" && <AuditsTab data={data} />}
    </div>
  );
}

// ─── Overview Tab ───

function OverviewTab({ data, verifiedPct }: { data: DashboardData; verifiedPct: number }) {
  const v = data.verification;
  const daysToFull = v.neverChecked > 0 ? Math.round((v.neverChecked / 600) * 10) / 10 : 0;

  return (
    <div className="space-y-6">
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
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
            <Row label="Verified (confirmed)" value={v.verified} color="text-green-600" />
            <Row label="Not Found (removed)" value={v.notFound} color="text-amber-600" />
            <Row label="Pending (adoption)" value={v.pending} color="text-blue-600" />
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
              ))}
          </div>
        </div>
      </div>

      {/* Source Breakdown */}
      <div className="bg-white rounded-lg border p-6">
        <h2 className="text-lg font-bold mb-4">Data Source Breakdown</h2>
        <div className="grid grid-cols-3 gap-4">
          <div>
            <p className="text-sm text-gray-500">RescueGroups</p>
            <p className="text-2xl font-bold text-blue-600">{data.sourceBreakdown.rescuegroups.toLocaleString()}</p>
            <p className="text-xs text-gray-400">{((data.sourceBreakdown.rescuegroups / Math.max(data.dogs.total, 1)) * 100).toFixed(1)}%</p>
          </div>
          <div>
            <p className="text-sm text-gray-500">Shelter Scrapers</p>
            <p className="text-2xl font-bold text-green-600">{data.sourceBreakdown.scraper.toLocaleString()}</p>
            <p className="text-xs text-gray-400">{((data.sourceBreakdown.scraper / Math.max(data.dogs.total, 1)) * 100).toFixed(1)}%</p>
          </div>
          <div>
            <p className="text-sm text-gray-500">Other</p>
            <p className="text-2xl font-bold text-gray-600">{data.sourceBreakdown.other.toLocaleString()}</p>
            <p className="text-xs text-gray-400">{((data.sourceBreakdown.other / Math.max(data.dogs.total, 1)) * 100).toFixed(1)}%</p>
          </div>
        </div>
      </div>
    </div>
  );
}

// ─── States Tab ───

function StatesTab({ data }: { data: DashboardData }) {
  const stateColumns: Column[] = [
    { key: "state", label: "State", render: (v) => <span className="font-medium">{String(v)}</span> },
    { key: "totalDogs", label: "Dogs", align: "right", render: (v) => Number(v).toLocaleString() },
    { key: "urgentDogs", label: "Urgent", align: "right", render: (v) => <span className={Number(v) > 0 ? "text-red-600 font-medium" : "text-gray-400"}>{Number(v)}</span> },
    { key: "shelterCount", label: "Shelters", align: "right", render: (v) => Number(v).toLocaleString() },
    {
      key: "urgentPct",
      label: "Urgent %",
      align: "right",
      render: (v) => {
        const pct = Number(v);
        return <span className={pct > 5 ? "text-red-600 font-medium" : pct > 1 ? "text-amber-600" : "text-gray-400"}>{pct.toFixed(1)}%</span>;
      },
    },
  ];

  return (
    <SortableTable
      columns={stateColumns}
      data={data.stateBreakdown}
      title="Dogs by State"
      searchKeys={["state"]}
      exportFilename="wtl-states"
      pageSize={50}
    />
  );
}

// ─── Shelters Tab ───

function SheltersTab({ data }: { data: DashboardData }) {
  const shelterColumns: Column[] = [
    {
      key: "name",
      label: "Shelter",
      render: (v, row) => (
        <div>
          <span className="font-medium">{String(v)}</span>
          <br />
          <span className="text-xs text-gray-400">{row.city}, {row.state}</span>
        </div>
      ),
    },
    { key: "dogCount", label: "Dogs", align: "right", render: (v) => Number(v).toLocaleString() },
    {
      key: "urgentCount",
      label: "Urgent",
      align: "right",
      render: (v) => <span className={Number(v) > 0 ? "text-red-600 font-medium" : "text-gray-400"}>{Number(v)}</span>,
    },
    {
      key: "platform",
      label: "Platform",
      render: (v) => v ? (
        <span className="px-2 py-0.5 rounded text-xs font-medium bg-blue-50 text-blue-700">
          {String(v).replace(/_/g, " ")}
        </span>
      ) : <span className="text-gray-300">none</span>,
    },
    {
      key: "lastScraped",
      label: "Last Scraped",
      align: "right",
      render: (v) => {
        if (!v) return <span className="text-gray-300">never</span>;
        const d = new Date(String(v));
        const ago = Math.round((Date.now() - d.getTime()) / (1000 * 60 * 60));
        return <span className={ago > 48 ? "text-amber-600" : "text-gray-500"}>{ago < 24 ? `${ago}h ago` : `${Math.round(ago / 24)}d ago`}</span>;
      },
    },
    {
      key: "orgType",
      label: "Type",
      render: (v) => <span className="text-xs text-gray-500">{String(v || "").replace(/_/g, " ")}</span>,
    },
  ];

  return (
    <SortableTable
      columns={shelterColumns}
      data={data.topShelters}
      title="Shelters with Dogs"
      searchKeys={["name", "city", "state", "platform"]}
      exportFilename="wtl-shelters"
      pageSize={25}
    />
  );
}

// ─── Trends Tab ───

function TrendsTab({ data }: { data: DashboardData }) {
  const trendColumns: Column[] = [
    { key: "stat_date", label: "Date", render: (v) => <span className="font-medium">{String(v)}</span> },
    { key: "total_available_dogs", label: "Dogs", align: "right", render: (v) => Number(v || 0).toLocaleString() },
    { key: "verification_coverage_pct", label: "Verified %", align: "right", render: (v) => `${Number(v || 0).toFixed(1)}%` },
    { key: "date_accuracy_pct", label: "Date Acc %", align: "right", render: (v) => `${Number(v || 0).toFixed(1)}%` },
    {
      key: "avg_wait_days_high_confidence",
      label: "Avg Wait",
      align: "right",
      render: (v, row) => `${Number(v || row.avg_wait_days_all || 0).toFixed(1)}d`,
    },
    { key: "dogs_added_today", label: "Added", align: "right", render: (v) => <span className="text-green-600">{Number(v || 0)}</span> },
    { key: "dogs_adopted_today", label: "Adopted", align: "right", render: (v) => <span className="text-blue-600">{Number(v || 0)}</span> },
  ];

  return (
    <SortableTable
      columns={trendColumns}
      data={data.dailyTrends}
      title="Daily Trends"
      exportFilename="wtl-daily-trends"
      pageSize={30}
    />
  );
}

// ─── Audits Tab ───

function AuditsTab({ data }: { data: DashboardData }) {
  const auditColumns: Column[] = [
    {
      key: "started_at",
      label: "Time",
      render: (v) => <span className="font-medium">{new Date(String(v)).toLocaleString()}</span>,
    },
    { key: "run_type", label: "Mode" },
    { key: "trigger_source", label: "Trigger", render: (v) => <span className="text-gray-500">{String(v || "")}</span> },
    {
      key: "status",
      label: "Status",
      align: "right",
      render: (v) => {
        const status = String(v || "");
        const cls = status === "completed" ? "bg-green-100 text-green-700"
          : status === "failed" ? "bg-red-100 text-red-700"
          : "bg-gray-100 text-gray-600";
        return <span className={`px-2 py-0.5 rounded text-xs font-medium ${cls}`}>{status}</span>;
      },
    },
    {
      key: "duration_ms",
      label: "Duration",
      align: "right",
      render: (v) => v ? `${(Number(v) / 1000).toFixed(1)}s` : "--",
    },
    {
      key: "checks_passed",
      label: "Passed",
      align: "right",
      render: (v) => <span className="text-green-600">{v ?? "--"}</span>,
    },
    {
      key: "checks_failed",
      label: "Failed",
      align: "right",
      render: (v) => <span className={Number(v) > 0 ? "text-red-600 font-medium" : "text-gray-400"}>{v ?? "--"}</span>,
    },
  ];

  return (
    <SortableTable
      columns={auditColumns}
      data={data.recentAudits}
      title="Recent Audit Runs"
      exportFilename="wtl-audits"
      pageSize={20}
    />
  );
}

// ─── Shared Components ───

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
      <span className={`font-medium ${color || "text-gray-900"}`}>
        {typeof value === "number" ? value.toLocaleString() : value}
      </span>
    </div>
  );
}

function ConfBar({ label, count, total, color }: { label: string; count: number; total: number; color: string }) {
  const pct = total > 0 ? (count / total) * 100 : 0;
  return (
    <div>
      <div className="flex justify-between text-sm mb-1">
        <span className="text-gray-600">{label}</span>
        <span className="text-gray-900 font-medium">
          {count.toLocaleString()} ({pct.toFixed(1)}%)
        </span>
      </div>
      <div className="h-2.5 bg-gray-100 rounded-full overflow-hidden">
        <div className={`h-full ${color} rounded-full`} style={{ width: `${pct}%` }} />
      </div>
    </div>
  );
}
