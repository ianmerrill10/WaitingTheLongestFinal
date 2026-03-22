"use client";

import { useState, useEffect, useCallback } from "react";
import Link from "next/link";

interface ScheduledPost {
  id: string;
  platform: string;
  caption: string;
  hashtags: string[];
  media_urls: string[];
  scheduled_at: string;
}

const PLATFORM_COLORS: Record<string, string> = {
  instagram: "bg-pink-100 text-pink-700 border-pink-300",
  facebook: "bg-blue-100 text-blue-700 border-blue-300",
  twitter: "bg-sky-100 text-sky-700 border-sky-300",
  tiktok: "bg-gray-800 text-white border-gray-600",
  bluesky: "bg-blue-50 text-blue-600 border-blue-200",
  youtube: "bg-red-100 text-red-700 border-red-300",
  reddit: "bg-orange-100 text-orange-700 border-orange-300",
  threads: "bg-gray-100 text-gray-700 border-gray-300",
};

export default function SocialCalendarPage() {
  const [posts, setPosts] = useState<ScheduledPost[]>([]);
  const [loading, setLoading] = useState(true);

  const fetchPosts = useCallback(async () => {
    setLoading(true);
    try {
      const res = await fetch("/api/social/schedule");
      if (res.ok) {
        const data = await res.json();
        setPosts(data.posts || []);
      }
    } catch {
      // fail silently
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchPosts();
  }, [fetchPosts]);

  const handleCancel = async (postId: string) => {
    if (!confirm("Cancel this scheduled post?")) return;
    try {
      const res = await fetch("/api/social/schedule", {
        method: "DELETE",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ post_id: postId }),
      });
      if (res.ok) fetchPosts();
    } catch {
      // fail silently
    }
  };

  // Group posts by date
  const groupedByDate = posts.reduce(
    (acc, post) => {
      const date = new Date(post.scheduled_at).toLocaleDateString("en-US", {
        weekday: "long",
        month: "long",
        day: "numeric",
      });
      if (!acc[date]) acc[date] = [];
      acc[date].push(post);
      return acc;
    },
    {} as Record<string, ScheduledPost[]>
  );

  return (
    <div className="min-h-screen bg-gray-50 py-8">
      <div className="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8">
        <Link
          href="/admin/social"
          className="text-sm text-green-700 hover:underline mb-1 inline-block"
        >
          &larr; Social Media
        </Link>
        <h1 className="text-2xl font-bold mb-6">Content Calendar</h1>

        {loading ? (
          <div className="bg-white rounded-lg border border-gray-200 p-12 text-center text-gray-400">
            Loading calendar...
          </div>
        ) : posts.length === 0 ? (
          <div className="bg-white rounded-lg border border-gray-200 p-12 text-center text-gray-500">
            <p className="text-lg mb-2">No scheduled posts</p>
            <p className="text-sm mb-4">
              Generate content and schedule it from the{" "}
              <Link
                href="/admin/social"
                className="text-green-700 hover:underline"
              >
                command center
              </Link>
              .
            </p>
          </div>
        ) : (
          <div className="space-y-6">
            {Object.entries(groupedByDate).map(([date, datePosts]) => (
              <div key={date}>
                <h2 className="text-sm font-semibold text-gray-500 uppercase mb-3">
                  {date}
                </h2>
                <div className="space-y-3">
                  {datePosts.map((post) => (
                    <div
                      key={post.id}
                      className="bg-white rounded-lg border border-gray-200 p-4"
                    >
                      <div className="flex items-start justify-between">
                        <div className="flex-1 min-w-0">
                          <div className="flex items-center gap-2 mb-2">
                            <span
                              className={`px-2 py-0.5 rounded text-xs font-medium border ${
                                PLATFORM_COLORS[post.platform] ||
                                PLATFORM_COLORS.threads
                              }`}
                            >
                              {post.platform}
                            </span>
                            <span className="text-xs text-gray-500">
                              {new Date(post.scheduled_at).toLocaleTimeString(
                                "en-US",
                                { hour: "numeric", minute: "2-digit" }
                              )}
                            </span>
                          </div>
                          <p className="text-sm text-gray-700 line-clamp-2">
                            {post.caption}
                          </p>
                        </div>
                        <button
                          onClick={() => handleCancel(post.id)}
                          className="ml-3 text-xs text-red-500 hover:underline"
                        >
                          Cancel
                        </button>
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
