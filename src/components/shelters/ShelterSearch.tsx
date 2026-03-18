"use client";

import { useRouter } from "next/navigation";
import { useState } from "react";

export default function ShelterSearch({
  initialQuery,
  initialState,
}: {
  initialQuery: string;
  initialState: string;
}) {
  const [query, setQuery] = useState(initialQuery);
  const router = useRouter();

  function handleSubmit(e: React.FormEvent) {
    e.preventDefault();
    const params = new URLSearchParams();
    if (query.trim()) params.set("q", query.trim());
    if (initialState) params.set("state", initialState);
    router.push(`/shelters?${params.toString()}`);
  }

  return (
    <div className="mb-8">
      <form onSubmit={handleSubmit} className="max-w-xl">
        <label htmlFor="shelter-search" className="sr-only">
          Search shelters
        </label>
        <div className="relative">
          <svg
            className="absolute left-3 top-1/2 -translate-y-1/2 w-5 h-5 text-gray-400"
            fill="none"
            viewBox="0 0 24 24"
            stroke="currentColor"
          >
            <path
              strokeLinecap="round"
              strokeLinejoin="round"
              strokeWidth={2}
              d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z"
            />
          </svg>
          <input
            id="shelter-search"
            type="text"
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            placeholder="Search by shelter name..."
            className="w-full pl-10 pr-20 py-3 border border-gray-300 rounded-lg text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
          />
          <button
            type="submit"
            className="absolute right-2 top-1/2 -translate-y-1/2 px-4 py-1.5 bg-blue-600 text-white text-sm font-medium rounded-md hover:bg-blue-700 transition"
          >
            Search
          </button>
        </div>
      </form>
    </div>
  );
}
