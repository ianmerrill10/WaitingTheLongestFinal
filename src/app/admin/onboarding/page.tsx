"use client";

import { useState, useEffect, useCallback } from "react";
import Link from "next/link";

interface PipelineStats {
  review: number;
  outreach: number;
  agreement: number;
  integration: number;
  testing: number;
  active: number;
  closed: number;
  total: number;
}

interface OnboardingApplication {
  id: string;
  shelter_id: string | null;
  org_name: string;
  org_type: string;
  ein: string | null;
  website: string | null;
  contact_first_name: string;
  contact_last_name: string;
  contact_email: string;
  contact_phone: string | null;
  city: string;
  state_code: string;
  zip_code: string;
  estimated_dog_count: number | null;
  current_software: string | null;
  integration_preference: string;
  status: string;
  stage: string;
  assigned_to: string | null;
  priority: string;
  score: number;
  source: string;
  internal_notes: string | null;
  rejection_reason: string | null;
  submitted_at: string;
  reviewed_at: string | null;
  approved_at: string | null;
  activated_at: string | null;
  created_at: string;
}

const STAGE_LABELS: Record<string, string> = {
  review: "Review",
  outreach: "Outreach",
  agreement: "Agreement",
  integration: "Integration",
  testing: "Testing",
  active: "Active",
  closed: "Closed",
};

const STAGE_COLORS: Record<string, string> = {
  review: "bg-yellow-100 text-yellow-800 border-yellow-300",
  outreach: "bg-blue-100 text-blue-800 border-blue-300",
  agreement: "bg-purple-100 text-purple-800 border-purple-300",
  integration: "bg-indigo-100 text-indigo-800 border-indigo-300",
  testing: "bg-orange-100 text-orange-800 border-orange-300",
  active: "bg-green-100 text-green-800 border-green-300",
  closed: "bg-gray-100 text-gray-600 border-gray-300",
};

const PRIORITY_COLORS: Record<string, string> = {
  urgent: "bg-red-600 text-white",
  high: "bg-red-100 text-red-700",
  normal: "bg-gray-100 text-gray-600",
  low: "bg-gray-50 text-gray-400",
};

export default function AdminOnboardingPage() {
  const [pipeline, setPipeline] = useState<PipelineStats | null>(null);
  const [applications, setApplications] = useState<OnboardingApplication[]>([]);
  const [priorityAlerts, setPriorityAlerts] = useState({ urgent: 0, high: 0 });
  const [loading, setLoading] = useState(true);
  const [stageFilter, setStageFilter] = useState<string>("");
  const [search, setSearch] = useState("");
  const [sort, setSort] = useState("newest");
  const [selectedApp, setSelectedApp] = useState<OnboardingApplication | null>(null);
  const [actionLoading, setActionLoading] = useState(false);

  const fetchData = useCallback(async () => {
    setLoading(true);
    try {
      const params = new URLSearchParams();
      if (stageFilter) params.set("stage", stageFilter);
      if (search) params.set("search", search);
      if (sort) params.set("sort", sort);

      const res = await fetch(`/api/admin/onboarding?${params}`);
      if (res.ok) {
        const data = await res.json();
        setPipeline(data.pipeline);
        setApplications(data.applications);
        setPriorityAlerts(data.priority_alerts);
      }
    } catch {
      // fail silently
    } finally {
      setLoading(false);
    }
  }, [stageFilter, search, sort]);

  useEffect(() => {
    fetchData();
  }, [fetchData]);

  const handleAction = async (
    id: string,
    action: string,
    extra?: Record<string, unknown>
  ) => {
    setActionLoading(true);
    try {
      const res = await fetch("/api/admin/onboarding", {
        method: "PATCH",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ id, action, ...extra }),
      });
      if (res.ok) {
        setSelectedApp(null);
        fetchData();
      }
    } catch {
      // fail silently
    } finally {
      setActionLoading(false);
    }
  };

  return (
    <div className="min-h-screen bg-gray-50 py-8">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="flex items-center justify-between mb-6">
          <div>
            <Link
              href="/admin"
              className="text-sm text-green-700 hover:underline mb-1 inline-block"
            >
              &larr; Admin
            </Link>
            <h1 className="text-2xl font-bold">Onboarding Pipeline</h1>
          </div>
          {priorityAlerts.urgent > 0 && (
            <div className="bg-red-50 border border-red-200 rounded-lg px-4 py-2 text-sm">
              <span className="font-bold text-red-700">
                {priorityAlerts.urgent} urgent
              </span>
              {priorityAlerts.high > 0 && (
                <span className="text-red-600 ml-2">
                  + {priorityAlerts.high} high priority
                </span>
              )}
            </div>
          )}
        </div>

        {/* Pipeline Funnel */}
        {pipeline && (
          <div className="grid grid-cols-3 sm:grid-cols-4 md:grid-cols-7 gap-2 mb-8">
            {(
              Object.keys(STAGE_LABELS) as Array<keyof typeof STAGE_LABELS>
            ).map((stage) => (
              <button
                key={stage}
                onClick={() =>
                  setStageFilter(stageFilter === stage ? "" : stage)
                }
                className={`rounded-lg border p-3 text-center transition-all ${
                  stageFilter === stage
                    ? STAGE_COLORS[stage] + " ring-2 ring-offset-1 ring-current"
                    : "bg-white border-gray-200 hover:border-gray-300"
                }`}
              >
                <div className="text-2xl font-bold">
                  {pipeline[stage as keyof PipelineStats]}
                </div>
                <div className="text-xs font-medium mt-0.5">
                  {STAGE_LABELS[stage]}
                </div>
              </button>
            ))}
          </div>
        )}

        {/* Filters */}
        <div className="flex flex-col sm:flex-row gap-3 mb-6">
          <input
            type="text"
            placeholder="Search by name, email..."
            value={search}
            onChange={(e) => setSearch(e.target.value)}
            className="form-input flex-1"
          />
          <select
            value={sort}
            onChange={(e) => setSort(e.target.value)}
            className="form-input w-auto"
          >
            <option value="newest">Newest First</option>
            <option value="oldest">Oldest First</option>
            <option value="priority">Priority Score</option>
            <option value="name">Name A-Z</option>
          </select>
        </div>

        {/* Applications List */}
        <div className="bg-white rounded-lg border border-gray-200 overflow-hidden">
          {loading ? (
            <div className="p-12 text-center text-gray-400">Loading pipeline...</div>
          ) : applications.length === 0 ? (
            <div className="p-12 text-center text-gray-500">
              <p className="text-lg mb-2">No applications found</p>
              <p className="text-sm">
                {stageFilter || search
                  ? "Try adjusting your filters."
                  : "Applications will appear here when shelters register."}
              </p>
            </div>
          ) : (
            <table className="w-full">
              <thead className="bg-gray-50 border-b border-gray-200">
                <tr>
                  <th className="text-left px-4 py-3 text-xs font-semibold text-gray-500 uppercase">
                    Organization
                  </th>
                  <th className="text-left px-4 py-3 text-xs font-semibold text-gray-500 uppercase hidden md:table-cell">
                    Contact
                  </th>
                  <th className="text-left px-4 py-3 text-xs font-semibold text-gray-500 uppercase">
                    Stage
                  </th>
                  <th className="text-left px-4 py-3 text-xs font-semibold text-gray-500 uppercase hidden sm:table-cell">
                    Priority
                  </th>
                  <th className="text-left px-4 py-3 text-xs font-semibold text-gray-500 uppercase hidden lg:table-cell">
                    Source
                  </th>
                  <th className="text-left px-4 py-3 text-xs font-semibold text-gray-500 uppercase hidden md:table-cell">
                    Submitted
                  </th>
                  <th className="text-right px-4 py-3 text-xs font-semibold text-gray-500 uppercase">
                    Actions
                  </th>
                </tr>
              </thead>
              <tbody className="divide-y divide-gray-100">
                {applications.map((app) => (
                  <tr key={app.id} className="hover:bg-gray-50">
                    <td className="px-4 py-3">
                      <div className="font-medium text-sm">{app.org_name}</div>
                      <div className="text-xs text-gray-500">
                        {app.city}, {app.state_code}
                        {app.estimated_dog_count
                          ? ` · ~${app.estimated_dog_count} dogs`
                          : ""}
                      </div>
                    </td>
                    <td className="px-4 py-3 hidden md:table-cell">
                      <div className="text-sm">
                        {app.contact_first_name} {app.contact_last_name}
                      </div>
                      <div className="text-xs text-gray-500">
                        {app.contact_email}
                      </div>
                    </td>
                    <td className="px-4 py-3">
                      <span
                        className={`inline-block px-2 py-0.5 rounded text-xs font-medium border ${
                          STAGE_COLORS[app.stage] || STAGE_COLORS.closed
                        }`}
                      >
                        {STAGE_LABELS[app.stage] || app.stage}
                      </span>
                    </td>
                    <td className="px-4 py-3 hidden sm:table-cell">
                      <span
                        className={`inline-block px-2 py-0.5 rounded text-xs font-medium ${
                          PRIORITY_COLORS[app.priority] || PRIORITY_COLORS.normal
                        }`}
                      >
                        {app.priority}
                      </span>
                      {app.score > 0 && (
                        <span className="text-xs text-gray-400 ml-1">
                          ({app.score})
                        </span>
                      )}
                    </td>
                    <td className="px-4 py-3 text-xs text-gray-500 hidden lg:table-cell">
                      {app.source.replace(/_/g, " ")}
                    </td>
                    <td className="px-4 py-3 text-xs text-gray-500 hidden md:table-cell">
                      {new Date(app.submitted_at).toLocaleDateString()}
                    </td>
                    <td className="px-4 py-3 text-right">
                      <button
                        onClick={() => setSelectedApp(app)}
                        className="text-xs text-green-700 hover:underline font-medium"
                      >
                        View
                      </button>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>

        {/* Detail Modal */}
        {selectedApp && (
          <div className="fixed inset-0 bg-black/50 z-50 flex items-center justify-center p-4">
            <div className="bg-white rounded-xl shadow-xl max-w-2xl w-full max-h-[90vh] overflow-y-auto">
              <div className="p-6 border-b border-gray-200">
                <div className="flex items-start justify-between">
                  <div>
                    <h2 className="text-xl font-bold">{selectedApp.org_name}</h2>
                    <p className="text-sm text-gray-500 mt-1">
                      {selectedApp.city}, {selectedApp.state_code}{" "}
                      {selectedApp.zip_code} · {selectedApp.org_type}
                    </p>
                  </div>
                  <button
                    onClick={() => setSelectedApp(null)}
                    className="text-gray-400 hover:text-gray-600 text-xl"
                  >
                    &times;
                  </button>
                </div>
              </div>

              <div className="p-6 space-y-6">
                {/* Status Row */}
                <div className="flex items-center gap-3">
                  <span
                    className={`px-3 py-1 rounded-full text-sm font-medium border ${
                      STAGE_COLORS[selectedApp.stage]
                    }`}
                  >
                    {STAGE_LABELS[selectedApp.stage]}
                  </span>
                  <span
                    className={`px-2 py-0.5 rounded text-xs font-medium ${
                      PRIORITY_COLORS[selectedApp.priority]
                    }`}
                  >
                    {selectedApp.priority} priority
                  </span>
                  <span className="text-xs text-gray-400">
                    Score: {selectedApp.score}
                  </span>
                </div>

                {/* Contact Info */}
                <div>
                  <h3 className="text-sm font-semibold text-gray-700 mb-2">
                    Contact
                  </h3>
                  <div className="grid grid-cols-2 gap-3 text-sm">
                    <div>
                      <span className="text-gray-500">Name:</span>{" "}
                      {selectedApp.contact_first_name}{" "}
                      {selectedApp.contact_last_name}
                    </div>
                    <div>
                      <span className="text-gray-500">Email:</span>{" "}
                      <a
                        href={`mailto:${selectedApp.contact_email}`}
                        className="text-green-700 hover:underline"
                      >
                        {selectedApp.contact_email}
                      </a>
                    </div>
                    {selectedApp.contact_phone && (
                      <div>
                        <span className="text-gray-500">Phone:</span>{" "}
                        {selectedApp.contact_phone}
                      </div>
                    )}
                    {selectedApp.website && (
                      <div>
                        <span className="text-gray-500">Website:</span>{" "}
                        <a
                          href={selectedApp.website}
                          target="_blank"
                          rel="noopener noreferrer"
                          className="text-green-700 hover:underline"
                        >
                          {selectedApp.website}
                        </a>
                      </div>
                    )}
                  </div>
                </div>

                {/* Details */}
                <div>
                  <h3 className="text-sm font-semibold text-gray-700 mb-2">
                    Details
                  </h3>
                  <div className="grid grid-cols-2 gap-3 text-sm">
                    {selectedApp.ein && (
                      <div>
                        <span className="text-gray-500">EIN:</span>{" "}
                        {selectedApp.ein}
                      </div>
                    )}
                    {selectedApp.estimated_dog_count && (
                      <div>
                        <span className="text-gray-500">Dogs:</span> ~
                        {selectedApp.estimated_dog_count}
                      </div>
                    )}
                    {selectedApp.current_software && (
                      <div>
                        <span className="text-gray-500">Software:</span>{" "}
                        {selectedApp.current_software}
                      </div>
                    )}
                    <div>
                      <span className="text-gray-500">Integration:</span>{" "}
                      {selectedApp.integration_preference}
                    </div>
                    <div>
                      <span className="text-gray-500">Source:</span>{" "}
                      {selectedApp.source.replace(/_/g, " ")}
                    </div>
                  </div>
                </div>

                {/* Timeline */}
                <div>
                  <h3 className="text-sm font-semibold text-gray-700 mb-2">
                    Timeline
                  </h3>
                  <div className="space-y-1 text-sm">
                    <div className="flex justify-between">
                      <span className="text-gray-500">Submitted</span>
                      <span>
                        {new Date(selectedApp.submitted_at).toLocaleString()}
                      </span>
                    </div>
                    {selectedApp.reviewed_at && (
                      <div className="flex justify-between">
                        <span className="text-gray-500">Reviewed</span>
                        <span>
                          {new Date(selectedApp.reviewed_at).toLocaleString()}
                        </span>
                      </div>
                    )}
                    {selectedApp.approved_at && (
                      <div className="flex justify-between">
                        <span className="text-gray-500">Approved</span>
                        <span>
                          {new Date(selectedApp.approved_at).toLocaleString()}
                        </span>
                      </div>
                    )}
                    {selectedApp.activated_at && (
                      <div className="flex justify-between">
                        <span className="text-gray-500">Activated</span>
                        <span>
                          {new Date(selectedApp.activated_at).toLocaleString()}
                        </span>
                      </div>
                    )}
                  </div>
                </div>

                {/* Internal Notes */}
                {selectedApp.internal_notes && (
                  <div>
                    <h3 className="text-sm font-semibold text-gray-700 mb-1">
                      Internal Notes
                    </h3>
                    <p className="text-sm text-gray-600 bg-gray-50 rounded p-3">
                      {selectedApp.internal_notes}
                    </p>
                  </div>
                )}

                {selectedApp.rejection_reason && (
                  <div>
                    <h3 className="text-sm font-semibold text-red-700 mb-1">
                      Rejection Reason
                    </h3>
                    <p className="text-sm text-red-600 bg-red-50 rounded p-3">
                      {selectedApp.rejection_reason}
                    </p>
                  </div>
                )}
              </div>

              {/* Action Buttons */}
              {selectedApp.stage !== "active" &&
                selectedApp.stage !== "closed" && (
                  <div className="p-6 border-t border-gray-200 flex flex-wrap gap-2">
                    {selectedApp.stage === "review" && (
                      <>
                        <button
                          onClick={() =>
                            handleAction(selectedApp.id, "review")
                          }
                          disabled={actionLoading}
                          className="px-4 py-2 bg-blue-600 text-white rounded-lg text-sm font-medium hover:bg-blue-700 disabled:opacity-50"
                        >
                          Mark Under Review
                        </button>
                        <button
                          onClick={() =>
                            handleAction(selectedApp.id, "approve")
                          }
                          disabled={actionLoading}
                          className="px-4 py-2 bg-green-600 text-white rounded-lg text-sm font-medium hover:bg-green-700 disabled:opacity-50"
                        >
                          Approve
                        </button>
                      </>
                    )}

                    {selectedApp.stage !== "review" && (
                      <button
                        onClick={() =>
                          handleAction(selectedApp.id, "advance", {
                            current_stage: selectedApp.stage,
                          })
                        }
                        disabled={actionLoading}
                        className="px-4 py-2 bg-green-600 text-white rounded-lg text-sm font-medium hover:bg-green-700 disabled:opacity-50"
                      >
                        Advance to Next Stage
                      </button>
                    )}

                    <button
                      onClick={() => {
                        const reason = prompt("Rejection reason:");
                        if (reason) {
                          handleAction(selectedApp.id, "reject", {
                            rejection_reason: reason,
                          });
                        }
                      }}
                      disabled={actionLoading}
                      className="px-4 py-2 bg-red-50 text-red-700 rounded-lg text-sm font-medium hover:bg-red-100 disabled:opacity-50"
                    >
                      Reject
                    </button>

                    <button
                      onClick={() => {
                        const priority = prompt(
                          "Set priority (low, normal, high, urgent):"
                        );
                        if (
                          priority &&
                          ["low", "normal", "high", "urgent"].includes(priority)
                        ) {
                          handleAction(selectedApp.id, "update", { priority });
                        }
                      }}
                      disabled={actionLoading}
                      className="px-4 py-2 bg-gray-100 text-gray-700 rounded-lg text-sm font-medium hover:bg-gray-200 disabled:opacity-50"
                    >
                      Set Priority
                    </button>
                  </div>
                )}
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
