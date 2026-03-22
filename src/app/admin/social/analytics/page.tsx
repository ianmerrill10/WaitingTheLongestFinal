"use client";

import { useState, useEffect, useCallback } from "react";
import Link from "next/link";

interface EngagementSummary {
  total_posts: number;
  total_likes: number;
  total_comments: number;
  total_shares: number;
  total_views: number;
  avg_engagement_rate: number;
  by_platform: Record<
    string,
    { posts: number; likes: number; shares: number; avg_score: number }
  >;
  by_content_type: Record<string, { posts: number; avg_score: number }>;
}

const PLATFORM_COLORS: Record<string, string> = {
  instagram: "bg-pink-500",
  facebook: "bg-blue-600",
  twitter: "bg-sky-500",
  tiktok: "bg-gray-800",
  bluesky: "bg-blue-400",
  youtube: "bg-red-600",
  reddit: "bg-orange-600",
};

export default function SocialAnalyticsPage() {
  const [summary, setSummary] = useState<EngagementSummary | null>(null);
  const [days, setDays] = useState(30);
  const [loading, setLoading] = useState(true);

  const fetchData = useCallback(async () => {
    setLoading(true);
    try {
      // Use the engagement tracker API (could be an admin endpoint)
      // For now, fetch via social schedule as placeholder
      const res = await fetch(`/api/social/schedule`);
      if (res.ok) {
        // Placeholder — real analytics would come from engagement-tracker
        setSummary({
          total_posts: 0,
          total_likes: 0,
          total_comments: 0,
          total_shares: 0,
          total_views: 0,
          avg_engagement_rate: 0,
          by_platform: {},
          by_content_type: {},
        });
      }
    } catch {
      // fail silently
    } finally {
      setLoading(false);
    }
  }, [days]);

  useEffect(() => {
    fetchData();
  }, [fetchData]);

  return (
    <div className="min-h-screen bg-gray-50 py-8">
      <div className="max-w-6xl mx-auto px-4 sm:px-6 lg:px-8">
        <Link
          href="/admin/social"
          className="text-sm text-green-700 hover:underline mb-1 inline-block"
        >
          &larr; Social Media
        </Link>
        <div className="flex items-center justify-between mb-6">
          <h1 className="text-2xl font-bold">Social Analytics</h1>
          <div className="flex border border-gray-200 rounded-lg overflow-hidden">
            {[7, 30, 90].map((d) => (
              <button
                key={d}
                onClick={() => setDays(d)}
                className={`px-3 py-1.5 text-sm font-medium transition-colors ${
                  days === d
                    ? "bg-green-700 text-white"
                    : "bg-white text-gray-600 hover:bg-gray-50"
                }`}
              >
                {d}d
              </button>
            ))}
          </div>
        </div>

        {loading ? (
          <div className="bg-white rounded-lg border p-12 text-center text-gray-400">
            Loading analytics...
          </div>
        ) : summary ? (
          <>
            {/* Overview Stats */}
            <div className="grid grid-cols-2 md:grid-cols-5 gap-4 mb-8">
              <StatBox label="Total Posts" value={summary.total_posts} />
              <StatBox
                label="Total Likes"
                value={summary.total_likes.toLocaleString()}
              />
              <StatBox
                label="Comments"
                value={summary.total_comments.toLocaleString()}
              />
              <StatBox
                label="Shares"
                value={summary.total_shares.toLocaleString()}
              />
              <StatBox
                label="Avg Engagement"
                value={`${summary.avg_engagement_rate}%`}
              />
            </div>

            {/* By Platform */}
            <div className="bg-white rounded-lg border border-gray-200 p-6 mb-6">
              <h2 className="text-sm font-semibold text-gray-700 mb-4">
                Performance by Platform
              </h2>
              {Object.keys(summary.by_platform).length === 0 ? (
                <p className="text-sm text-gray-400">
                  No platform data yet. Start posting to see analytics.
                </p>
              ) : (
                <div className="space-y-3">
                  {Object.entries(summary.by_platform).map(
                    ([platform, stats]) => (
                      <div
                        key={platform}
                        className="flex items-center gap-3"
                      >
                        <div
                          className={`w-3 h-3 rounded-full ${
                            PLATFORM_COLORS[platform] || "bg-gray-400"
                          }`}
                        />
                        <span className="text-sm font-medium w-24 capitalize">
                          {platform}
                        </span>
                        <span className="text-sm text-gray-500">
                          {stats.posts} posts
                        </span>
                        <span className="text-sm text-gray-500">
                          {stats.likes} likes
                        </span>
                        <span className="text-sm text-gray-500">
                          {stats.shares} shares
                        </span>
                        <span className="text-sm font-medium text-green-600 ml-auto">
                          {stats.avg_score}% avg
                        </span>
                      </div>
                    )
                  )}
                </div>
              )}
            </div>

            {/* By Content Type */}
            <div className="bg-white rounded-lg border border-gray-200 p-6">
              <h2 className="text-sm font-semibold text-gray-700 mb-4">
                Performance by Content Type
              </h2>
              {Object.keys(summary.by_content_type).length === 0 ? (
                <p className="text-sm text-gray-400">
                  No content data yet. Generate and post content to see what
                  performs best.
                </p>
              ) : (
                <div className="space-y-3">
                  {Object.entries(summary.by_content_type)
                    .sort(([, a], [, b]) => b.avg_score - a.avg_score)
                    .map(([type, stats]) => (
                      <div key={type} className="flex items-center gap-3">
                        <span className="text-sm font-medium w-40 capitalize">
                          {type.replace(/_/g, " ")}
                        </span>
                        <span className="text-sm text-gray-500">
                          {stats.posts} posts
                        </span>
                        <div className="flex-1 bg-gray-100 rounded-full h-2">
                          <div
                            className="bg-green-500 rounded-full h-2"
                            style={{
                              width: `${Math.min(stats.avg_score * 10, 100)}%`,
                            }}
                          />
                        </div>
                        <span className="text-sm font-medium text-green-600">
                          {stats.avg_score}%
                        </span>
                      </div>
                    ))}
                </div>
              )}
            </div>
          </>
        ) : (
          <div className="bg-white rounded-lg border p-12 text-center text-gray-500">
            No analytics data available.
          </div>
        )}
      </div>
    </div>
  );
}

function StatBox({
  label,
  value,
}: {
  label: string;
  value: string | number;
}) {
  return (
    <div className="bg-white rounded-lg border border-gray-200 p-4 text-center">
      <div className="text-2xl font-bold">{value}</div>
      <div className="text-xs text-gray-500">{label}</div>
    </div>
  );
}
