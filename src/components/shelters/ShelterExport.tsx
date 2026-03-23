"use client";

import { useState } from "react";

interface ShelterExportProps {
  filterParams: string;
  total: number;
}

export default function ShelterExport({ filterParams, total }: ShelterExportProps) {
  const [exporting, setExporting] = useState(false);

  async function handleExport() {
    if (exporting || total === 0) return;
    setExporting(true);

    try {
      // Fetch all matching shelters (up to 10k)
      const params = new URLSearchParams(filterParams);
      params.set("limit", "10000");
      params.set("page", "1");

      const res = await fetch(`/api/shelters?${params.toString()}`);
      if (!res.ok) throw new Error("Export failed");

      const data = await res.json();
      const shelters = data.shelters || [];

      // Build CSV
      const headers = [
        "Name", "City", "State", "Zip", "County", "Type", "Kill Shelter",
        "Accepts Rescue Pulls", "Verified", "Available Dogs", "Urgent Dogs",
        "Phone", "Email", "Website", "Platform", "EIN", "Last Scraped",
      ];

      const rows = shelters.map((s: Record<string, unknown>) => [
        csvEscape(s.name as string),
        csvEscape(s.city as string),
        s.state_code,
        s.zip_code || "",
        csvEscape(s.county as string || ""),
        s.shelter_type || "",
        s.is_kill_shelter ? "Yes" : "No",
        s.accepts_rescue_pulls ? "Yes" : "No",
        s.is_verified ? "Yes" : "No",
        s.available_dog_count || 0,
        s.urgent_dog_count || 0,
        s.phone || "",
        s.email || "",
        s.website || "",
        s.website_platform || "",
        s.ein || "",
        s.last_scraped_at ? new Date(s.last_scraped_at as string).toISOString().split("T")[0] : "",
      ]);

      const csv = [headers.join(","), ...rows.map((r: string[]) => r.join(","))].join("\n");
      const blob = new Blob([csv], { type: "text/csv;charset=utf-8;" });
      const url = URL.createObjectURL(blob);
      const a = document.createElement("a");
      a.href = url;
      a.download = `wtl-shelters-${new Date().toISOString().split("T")[0]}.csv`;
      a.click();
      URL.revokeObjectURL(url);
    } catch {
      alert("Export failed. Please try again.");
    } finally {
      setExporting(false);
    }
  }

  return (
    <button
      onClick={handleExport}
      disabled={exporting || total === 0}
      className="inline-flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium bg-white border border-gray-300 rounded-md hover:bg-gray-50 disabled:opacity-50 disabled:cursor-not-allowed transition"
      title={`Export ${total.toLocaleString()} shelters to CSV`}
    >
      <svg className="w-3.5 h-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
        <path strokeLinecap="round" strokeLinejoin="round" d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4" />
      </svg>
      {exporting ? "Exporting..." : "Export CSV"}
    </button>
  );
}

function csvEscape(value: string | null | undefined): string {
  if (!value) return "";
  if (value.includes(",") || value.includes('"') || value.includes("\n")) {
    return `"${value.replace(/"/g, '""')}"`;
  }
  return value;
}
