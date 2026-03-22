"use client";

import { useState, useEffect, useCallback } from "react";
import Link from "next/link";

interface AnalyticsData {
  totalDogs: number;
  activeDogs: number;
  adoptedDogs: number;
  totalViews: number;
  avgWaitDays: number;
  urgentCount: number;
  weeklyAdoptions: number[];
  weeklyViews: number[];
  topDogs: Array<{
    id: string;
    name: string;
    views: number;
    status: string;
  }>;
}

export default function DashboardAnalyticsPage() {
  const [analytics, setAnalytics] = useState<AnalyticsData | null>(null);
  const [loading, setLoading] = useState(true);
  const [timeRange, setTimeRange] = useState<"7d" | "30d" | "90d">("30d");

  const fetchAnalytics = useCallback(async () => {
    setLoading(true);
    try {
      const shelterId = document.cookie
        .split("; ")
        .find((c) => c.startsWith("wtl_shelter_id="))
        ?.split("=")[1];
      if (!shelterId) { setLoading(false); return; }

      const res = await fetch(`/api/admin/partner?shelter_id=${shelterId}`);
      if (res.ok) {
        const data = await res.json();
        setAnalytics({
          totalDogs: data.stats.total_dogs,
          activeDogs: data.stats.available_dogs,
          adoptedDogs: data.stats.adopted_dogs,
          totalViews: data.stats.total_views,
          avgWaitDays: 0,
          urgentCount: data.stats.urgent_dogs,
          weeklyAdoptions: [],
          weeklyViews: [],
          topDogs: data.topDogs || [],
        });
      }
    } catch {
      // silently fail — dashboard shows empty state
    } finally {
      setLoading(false);
    }
  }, [timeRange]);

  useEffect(() => {
    fetchAnalytics();
  }, [fetchAnalytics]);

  return (
    <div className="min-h-screen bg-gray-50 py-8">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <Link
          href="/partners/dashboard"
          className="text-sm text-green-700 hover:underline mb-1 inline-block"
        >
          &larr; Dashboard
        </Link>
        <div className="flex items-center justify-between mb-6">
          <h1 className="text-2xl font-bold">Analytics</h1>
          <div className="flex border border-gray-200 rounded-lg overflow-hidden">
            {(["7d", "30d", "90d"] as const).map((range) => (
              <button
                key={range}
                onClick={() => setTimeRange(range)}
                className={`px-3 py-1.5 text-sm font-medium transition-colors ${
                  timeRange === range
                    ? "bg-green-700 text-white"
                    : "bg-white text-gray-600 hover:bg-gray-50"
                }`}
              >
                {range === "7d" ? "7 Days" : range === "30d" ? "30 Days" : "90 Days"}
              </button>
            ))}
          </div>
        </div>

        {loading ? (
          <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-8">
            {Array.from({ length: 4 }).map((_, i) => (
              <div
                key={i}
                className="bg-white rounded-lg border border-gray-200 p-5 animate-pulse"
              >
                <div className="h-4 bg-gray-200 rounded w-20 mb-3" />
                <div className="h-8 bg-gray-200 rounded w-16" />
              </div>
            ))}
          </div>
        ) : analytics ? (
          <>
            {/* Stats Grid */}
            <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-8">
              <StatCard label="Active Dogs" value={analytics.activeDogs} />
              <StatCard
                label="Total Views"
                value={analytics.totalViews.toLocaleString()}
              />
              <StatCard
                label="Adopted"
                value={analytics.adoptedDogs}
                accent="green"
              />
              <StatCard
                label="Avg Wait"
                value={`${analytics.avgWaitDays}d`}
              />
            </div>

            {/* Charts Row */}
            <div className="grid md:grid-cols-2 gap-6 mb-8">
              {/* Weekly Adoptions Bar Chart */}
              <div className="bg-white rounded-lg border border-gray-200 p-5">
                <h2 className="text-sm font-semibold text-gray-700 mb-4">
                  Weekly Adoptions
                </h2>
                <div className="flex items-end gap-1 h-32">
                  {(analytics.weeklyAdoptions.length > 0
                    ? analytics.weeklyAdoptions
                    : [0, 0, 0, 0]
                  ).map((count, i) => {
                    const max = Math.max(
                      ...analytics.weeklyAdoptions,
                      1
                    );
                    const height = (count / max) * 100;
                    return (
                      <div
                        key={i}
                        className="flex-1 flex flex-col items-center"
                      >
                        <span className="text-xs text-gray-500 mb-1">
                          {count}
                        </span>
                        <div
                          className="w-full bg-green-500 rounded-t"
                          style={{ height: `${Math.max(height, 4)}%` }}
                        />
                        <span className="text-xs text-gray-400 mt-1">
                          W{i + 1}
                        </span>
                      </div>
                    );
                  })}
                </div>
              </div>

              {/* Weekly Views Bar Chart */}
              <div className="bg-white rounded-lg border border-gray-200 p-5">
                <h2 className="text-sm font-semibold text-gray-700 mb-4">
                  Weekly Views
                </h2>
                <div className="flex items-end gap-1 h-32">
                  {(analytics.weeklyViews.length > 0
                    ? analytics.weeklyViews
                    : [0, 0, 0, 0]
                  ).map((count, i) => {
                    const max = Math.max(...analytics.weeklyViews, 1);
                    const height = (count / max) * 100;
                    return (
                      <div
                        key={i}
                        className="flex-1 flex flex-col items-center"
                      >
                        <span className="text-xs text-gray-500 mb-1">
                          {count}
                        </span>
                        <div
                          className="w-full bg-blue-500 rounded-t"
                          style={{ height: `${Math.max(height, 4)}%` }}
                        />
                        <span className="text-xs text-gray-400 mt-1">
                          W{i + 1}
                        </span>
                      </div>
                    );
                  })}
                </div>
              </div>
            </div>

            {/* Urgency Summary */}
            <div className="bg-white rounded-lg border border-gray-200 p-5 mb-8">
              <h2 className="text-sm font-semibold text-gray-700 mb-3">
                Urgency Summary
              </h2>
              <div className="flex items-center gap-6">
                <div>
                  <span className="text-2xl font-bold text-red-600">
                    {analytics.urgentCount}
                  </span>
                  <span className="text-sm text-gray-500 ml-2">
                    dogs at critical/high urgency
                  </span>
                </div>
                {analytics.urgentCount > 0 && (
                  <Link
                    href="/partners/dashboard/dogs?urgency=critical,high"
                    className="text-sm text-red-600 hover:underline font-medium"
                  >
                    View urgent dogs &rarr;
                  </Link>
                )}
              </div>
            </div>

            {/* Top Viewed Dogs */}
            <div className="bg-white rounded-lg border border-gray-200 overflow-hidden">
              <div className="px-5 py-4 border-b border-gray-200">
                <h2 className="text-sm font-semibold text-gray-700">
                  Most Viewed Dogs
                </h2>
              </div>
              {analytics.topDogs.length === 0 ? (
                <div className="p-8 text-center text-gray-500 text-sm">
                  No view data available yet.
                </div>
              ) : (
                <table className="w-full">
                  <thead className="bg-gray-50 border-b border-gray-200">
                    <tr>
                      <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">
                        Dog
                      </th>
                      <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">
                        Views
                      </th>
                      <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">
                        Status
                      </th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-gray-100">
                    {analytics.topDogs.map((dog) => (
                      <tr key={dog.id} className="hover:bg-gray-50">
                        <td className="px-4 py-3 text-sm font-medium">
                          <Link
                            href={`/dogs/${dog.id}`}
                            className="text-green-700 hover:underline"
                          >
                            {dog.name}
                          </Link>
                        </td>
                        <td className="px-4 py-3 text-sm">
                          {dog.views.toLocaleString()}
                        </td>
                        <td className="px-4 py-3">
                          <span
                            className={`px-2 py-0.5 rounded text-xs font-medium ${
                              dog.status === "available"
                                ? "bg-green-100 text-green-700"
                                : dog.status === "adopted"
                                  ? "bg-blue-100 text-blue-700"
                                  : "bg-gray-100 text-gray-600"
                            }`}
                          >
                            {dog.status}
                          </span>
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              )}
            </div>
          </>
        ) : (
          <div className="bg-white rounded-lg border border-gray-200 p-12 text-center text-gray-500">
            <p className="text-lg mb-2">No analytics data yet</p>
            <p className="text-sm">
              Analytics will populate as your dogs receive views and interactions.
            </p>
          </div>
        )}
      </div>
    </div>
  );
}

function StatCard({
  label,
  value,
  accent,
}: {
  label: string;
  value: string | number;
  accent?: "green" | "red";
}) {
  return (
    <div className="bg-white rounded-lg border border-gray-200 p-5">
      <p className="text-sm text-gray-500 mb-1">{label}</p>
      <p
        className={`text-2xl font-bold ${
          accent === "green"
            ? "text-green-600"
            : accent === "red"
              ? "text-red-600"
              : "text-gray-900"
        }`}
      >
        {value}
      </p>
    </div>
  );
}
