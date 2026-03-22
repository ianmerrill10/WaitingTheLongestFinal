"use client";

import { useState } from "react";
import Link from "next/link";

const PLATFORMS = [
  "instagram", "facebook", "twitter", "tiktok",
  "bluesky", "youtube", "reddit", "threads",
] as const;

const CONTENT_TYPES = [
  { value: "dog_spotlight", label: "Dog Spotlight" },
  { value: "urgent_alert", label: "Urgent Alert" },
  { value: "longest_waiting", label: "Longest Waiting" },
  { value: "new_arrivals", label: "New Arrivals" },
  { value: "foster_appeal", label: "Foster Appeal" },
  { value: "happy_tails", label: "Happy Tails" },
  { value: "breed_facts", label: "Breed Facts" },
  { value: "shelter_spotlight", label: "Shelter Spotlight" },
] as const;

interface GeneratedContent {
  caption: string;
  hashtags: string[];
  media_urls: string[];
  media_type: string;
  content_type: string;
  dog_id: string | null;
  platform: string;
}

export default function SocialCommandCenter() {
  const [platform, setPlatform] = useState<string>("instagram");
  const [contentType, setContentType] = useState<string>("dog_spotlight");
  const [generating, setGenerating] = useState(false);
  const [content, setContent] = useState<GeneratedContent | null>(null);
  const [publishStatus, setPublishStatus] = useState<string | null>(null);

  const handleGenerate = async () => {
    setGenerating(true);
    setContent(null);
    setPublishStatus(null);
    try {
      const res = await fetch("/api/social/generate", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ platform, content_type: contentType }),
      });
      if (res.ok) {
        const data = await res.json();
        setContent(data.content);
      }
    } catch {
      // fail silently
    } finally {
      setGenerating(false);
    }
  };

  return (
    <div className="min-h-screen bg-gray-50 py-8">
      <div className="max-w-6xl mx-auto px-4 sm:px-6 lg:px-8">
        <Link
          href="/admin"
          className="text-sm text-green-700 hover:underline mb-1 inline-block"
        >
          &larr; Admin
        </Link>
        <div className="flex items-center justify-between mb-6">
          <h1 className="text-2xl font-bold">Social Media Command Center</h1>
          <div className="flex gap-2">
            <Link
              href="/admin/social/calendar"
              className="text-sm bg-white border border-gray-200 rounded-lg px-3 py-1.5 hover:bg-gray-50"
            >
              Calendar
            </Link>
            <Link
              href="/admin/social/analytics"
              className="text-sm bg-white border border-gray-200 rounded-lg px-3 py-1.5 hover:bg-gray-50"
            >
              Analytics
            </Link>
          </div>
        </div>

        {/* Generator */}
        <div className="bg-white rounded-lg border border-gray-200 p-6 mb-6">
          <h2 className="text-lg font-semibold mb-4">Generate Content</h2>

          <div className="grid sm:grid-cols-3 gap-4 mb-4">
            <div>
              <label className="block text-sm font-medium text-gray-700 mb-1">
                Platform
              </label>
              <select
                value={platform}
                onChange={(e) => setPlatform(e.target.value)}
                className="form-input"
              >
                {PLATFORMS.map((p) => (
                  <option key={p} value={p}>
                    {p.charAt(0).toUpperCase() + p.slice(1)}
                  </option>
                ))}
              </select>
            </div>

            <div>
              <label className="block text-sm font-medium text-gray-700 mb-1">
                Content Type
              </label>
              <select
                value={contentType}
                onChange={(e) => setContentType(e.target.value)}
                className="form-input"
              >
                {CONTENT_TYPES.map((ct) => (
                  <option key={ct.value} value={ct.value}>
                    {ct.label}
                  </option>
                ))}
              </select>
            </div>

            <div className="flex items-end">
              <button
                onClick={handleGenerate}
                disabled={generating}
                className="w-full py-2 bg-green-700 text-white rounded-lg font-medium hover:bg-green-800 disabled:opacity-50"
              >
                {generating ? "Generating..." : "Generate"}
              </button>
            </div>
          </div>
        </div>

        {/* Generated Content Preview */}
        {content && (
          <div className="bg-white rounded-lg border border-gray-200 p-6 mb-6">
            <div className="flex items-center justify-between mb-4">
              <h2 className="text-lg font-semibold">Preview</h2>
              <span className="text-xs bg-blue-100 text-blue-700 px-2 py-0.5 rounded">
                {content.platform} · {content.content_type.replace(/_/g, " ")}
              </span>
            </div>

            {/* Photo Preview */}
            {content.media_urls.length > 0 && (
              <div className="flex gap-2 mb-4 overflow-x-auto">
                {content.media_urls.map((url, i) => (
                  <img
                    key={i}
                    src={url}
                    alt={`Media ${i + 1}`}
                    className="h-32 w-32 object-cover rounded-lg border"
                  />
                ))}
              </div>
            )}

            {/* Caption */}
            <div className="bg-gray-50 rounded-lg p-4 mb-4">
              <p className="text-sm whitespace-pre-wrap">{content.caption}</p>
            </div>

            {/* Hashtags */}
            <div className="flex flex-wrap gap-1 mb-4">
              {content.hashtags.map((tag, i) => (
                <span
                  key={i}
                  className="text-xs bg-blue-50 text-blue-600 px-2 py-0.5 rounded"
                >
                  #{tag}
                </span>
              ))}
            </div>

            {/* Actions */}
            <div className="flex gap-2">
              <button
                onClick={handleGenerate}
                className="px-4 py-2 bg-gray-100 text-gray-700 rounded-lg text-sm hover:bg-gray-200"
              >
                Regenerate
              </button>
              <button
                onClick={() => {
                  const fullText = `${content.caption}\n\n${content.hashtags.map(h => `#${h}`).join(" ")}`;
                  navigator.clipboard.writeText(fullText);
                  setPublishStatus("Copied to clipboard!");
                }}
                className="px-4 py-2 bg-blue-600 text-white rounded-lg text-sm hover:bg-blue-700"
              >
                Copy to Clipboard
              </button>
            </div>

            {publishStatus && (
              <p className="text-sm text-green-600 mt-2">{publishStatus}</p>
            )}
          </div>
        )}

        {/* Quick Stats */}
        <div className="grid sm:grid-cols-4 gap-4">
          <div className="bg-white rounded-lg border border-gray-200 p-4 text-center">
            <div className="text-2xl font-bold text-pink-600">0</div>
            <div className="text-xs text-gray-500">Instagram Posts</div>
          </div>
          <div className="bg-white rounded-lg border border-gray-200 p-4 text-center">
            <div className="text-2xl font-bold text-blue-600">0</div>
            <div className="text-xs text-gray-500">Facebook Posts</div>
          </div>
          <div className="bg-white rounded-lg border border-gray-200 p-4 text-center">
            <div className="text-2xl font-bold text-sky-500">0</div>
            <div className="text-xs text-gray-500">Twitter Posts</div>
          </div>
          <div className="bg-white rounded-lg border border-gray-200 p-4 text-center">
            <div className="text-2xl font-bold text-gray-800">0</div>
            <div className="text-xs text-gray-500">Total Engagement</div>
          </div>
        </div>
      </div>
    </div>
  );
}
