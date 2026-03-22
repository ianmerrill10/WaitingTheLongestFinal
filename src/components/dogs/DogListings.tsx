"use client";

import { useEffect, useState, useCallback } from "react";
import { useRouter, useSearchParams } from "next/navigation";
import DogGrid from "./DogGrid";
import Pagination from "@/components/ui/Pagination";
import {
  DOG_SIZES,
  DOG_SIZE_LABELS,
  DOG_AGES,
  DOG_AGE_LABELS,
} from "@/lib/constants";
import type { Dog } from "@/types/dog";

const SORT_OPTIONS = [
  { value: "wait_time", label: "Longest Waiting" },
  { value: "urgency", label: "Most Urgent" },
  { value: "newest", label: "Newest" },
  { value: "name", label: "Name (A-Z)" },
] as const;

const US_STATES = [
  "AL","AK","AZ","AR","CA","CO","CT","DE","FL","GA",
  "HI","ID","IL","IN","IA","KS","KY","LA","ME","MD",
  "MA","MI","MN","MS","MO","MT","NE","NV","NH","NJ",
  "NM","NY","NC","ND","OH","OK","OR","PA","RI","SC",
  "SD","TN","TX","UT","VT","VA","WA","WV","WI","WY",
];

interface DogsResponse {
  dogs: (Dog & { shelters?: { name: string; city: string; state_code: string } })[];
  total: number;
  page: number;
  limit: number;
  totalPages: number;
}

export default function DogListings() {
  const router = useRouter();
  const searchParams = useSearchParams();

  const [dogs, setDogs] = useState<DogsResponse["dogs"]>([]);
  const [total, setTotal] = useState(0);
  const [totalPages, setTotalPages] = useState(0);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  // Read filters from URL
  const currentPage = parseInt(searchParams.get("page") || "1");
  const sort = searchParams.get("sort") || "wait_time";
  const breed = searchParams.get("breed") || "";
  const size = searchParams.get("size") || "";
  const age = searchParams.get("age") || "";
  const gender = searchParams.get("gender") || "";
  const state = searchParams.get("state") || "";
  const urgency = searchParams.get("urgency") || "";
  const search = searchParams.get("q") || "";

  const fetchDogs = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const params = new URLSearchParams();
      params.set("page", String(currentPage));
      params.set("sort", sort);
      if (breed) params.set("breed", breed);
      if (size) params.set("size", size);
      if (age) params.set("age", age);
      if (gender) params.set("gender", gender);
      if (state) params.set("state", state);
      if (urgency) params.set("urgency", urgency);
      if (search) params.set("q", search);

      // Pass through compatibility & quick filters
      const passthrough = ["good_with_kids", "good_with_dogs", "good_with_cats", "house_trained", "new_today", "fee_waived"];
      for (const key of passthrough) {
        const val = searchParams.get(key);
        if (val) params.set(key, val);
      }

      const res = await fetch(`/api/dogs?${params.toString()}`);
      if (!res.ok) throw new Error("Failed to fetch dogs");
      const data: DogsResponse = await res.json();
      setDogs(data.dogs);
      setTotal(data.total);
      setTotalPages(data.totalPages);
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed to load dogs");
    } finally {
      setLoading(false);
    }
  }, [currentPage, sort, breed, size, age, gender, state, urgency, search, searchParams]);

  useEffect(() => {
    fetchDogs();
  }, [fetchDogs]);

  // Update URL params
  function updateFilter(key: string, value: string) {
    const params = new URLSearchParams(searchParams.toString());
    if (value) {
      params.set(key, value);
    } else {
      params.delete(key);
    }
    params.set("page", "1"); // Reset to page 1 on filter change
    router.push(`/dogs?${params.toString()}`);
  }

  function clearFilters() {
    router.push("/dogs");
  }

  function handlePageChange(page: number) {
    const params = new URLSearchParams(searchParams.toString());
    params.set("page", String(page));
    router.push(`/dogs?${params.toString()}`);
  }

  const hasFilters = breed || size || age || gender || state || urgency || search ||
    searchParams.get("good_with_kids") || searchParams.get("good_with_dogs") ||
    searchParams.get("good_with_cats") || searchParams.get("house_trained") ||
    searchParams.get("new_today") || searchParams.get("fee_waived");

  return (
    <div className="flex flex-col lg:flex-row gap-8">
      {/* Filter Sidebar */}
      <aside className="w-full lg:w-64 flex-shrink-0">
        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-5 sticky top-20">
          <div className="flex items-center justify-between mb-4">
            <h2 className="font-bold text-lg text-gray-900">Filters</h2>
            {hasFilters && (
              <button
                onClick={clearFilters}
                className="text-xs text-blue-600 hover:text-blue-800"
              >
                Clear all
              </button>
            )}
          </div>

          {/* Search */}
          <div className="mb-5">
            <label
              htmlFor="search-filter"
              className="block text-sm font-semibold text-gray-700 mb-1"
            >
              Search
            </label>
            <input
              id="search-filter"
              type="text"
              placeholder="Name or breed..."
              value={search}
              onChange={(e) => updateFilter("q", e.target.value)}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            />
          </div>

          {/* Breed Filter */}
          <div className="mb-5">
            <label
              htmlFor="breed-filter"
              className="block text-sm font-semibold text-gray-700 mb-1"
            >
              Breed
            </label>
            <input
              id="breed-filter"
              type="text"
              placeholder="e.g. Labrador..."
              value={breed}
              onChange={(e) => updateFilter("breed", e.target.value)}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            />
          </div>

          {/* Size Filter */}
          <div className="mb-5">
            <label
              htmlFor="size-filter"
              className="block text-sm font-semibold text-gray-700 mb-1"
            >
              Size
            </label>
            <select
              id="size-filter"
              value={size}
              onChange={(e) => updateFilter("size", e.target.value)}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            >
              <option value="">All Sizes</option>
              {DOG_SIZES.map((s) => (
                <option key={s} value={s}>
                  {DOG_SIZE_LABELS[s]}
                </option>
              ))}
            </select>
          </div>

          {/* Age Filter */}
          <div className="mb-5">
            <label
              htmlFor="age-filter"
              className="block text-sm font-semibold text-gray-700 mb-1"
            >
              Age
            </label>
            <select
              id="age-filter"
              value={age}
              onChange={(e) => updateFilter("age", e.target.value)}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            >
              <option value="">All Ages</option>
              {DOG_AGES.map((a) => (
                <option key={a} value={a}>
                  {DOG_AGE_LABELS[a]}
                </option>
              ))}
            </select>
          </div>

          {/* Gender Filter */}
          <div className="mb-5">
            <label
              htmlFor="gender-filter"
              className="block text-sm font-semibold text-gray-700 mb-1"
            >
              Gender
            </label>
            <select
              id="gender-filter"
              value={gender}
              onChange={(e) => updateFilter("gender", e.target.value)}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            >
              <option value="">All</option>
              <option value="male">Male</option>
              <option value="female">Female</option>
            </select>
          </div>

          {/* State Filter */}
          <div className="mb-5">
            <label
              htmlFor="state-filter"
              className="block text-sm font-semibold text-gray-700 mb-1"
            >
              State
            </label>
            <select
              id="state-filter"
              value={state}
              onChange={(e) => updateFilter("state", e.target.value)}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            >
              <option value="">All States</option>
              {US_STATES.map((code) => (
                <option key={code} value={code}>
                  {code}
                </option>
              ))}
            </select>
          </div>

          {/* Urgency Filter */}
          <div className="mb-5">
            <label
              htmlFor="urgency-filter"
              className="block text-sm font-semibold text-gray-700 mb-1"
            >
              Urgency
            </label>
            <select
              id="urgency-filter"
              value={urgency}
              onChange={(e) => updateFilter("urgency", e.target.value)}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            >
              <option value="">All</option>
              <option value="critical">Critical (&lt;24h)</option>
              <option value="high">Urgent (&lt;72h)</option>
              <option value="medium">At Risk (&lt;7d)</option>
              <option value="normal">Available</option>
            </select>
          </div>

          {/* Compatibility Filters */}
          <div className="mb-5">
            <p className="block text-sm font-semibold text-gray-700 mb-2">Compatibility</p>
            <div className="space-y-2">
              {[
                { key: "good_with_kids", label: "Good with Kids" },
                { key: "good_with_dogs", label: "Good with Dogs" },
                { key: "good_with_cats", label: "Good with Cats" },
                { key: "house_trained", label: "House Trained" },
              ].map(({ key, label }) => (
                <label key={key} className="flex items-center gap-2 text-sm text-gray-700 cursor-pointer">
                  <input
                    type="checkbox"
                    checked={searchParams.get(key) === "true"}
                    onChange={(e) => updateFilter(key, e.target.checked ? "true" : "")}
                    className="rounded border-gray-300 text-blue-600 focus:ring-blue-500"
                  />
                  {label}
                </label>
              ))}
            </div>
          </div>

          {/* Quick Filters */}
          <div className="mb-5">
            <p className="block text-sm font-semibold text-gray-700 mb-2">Quick Filters</p>
            <div className="space-y-2">
              <label className="flex items-center gap-2 text-sm text-gray-700 cursor-pointer">
                <input
                  type="checkbox"
                  checked={searchParams.get("new_today") === "true"}
                  onChange={(e) => updateFilter("new_today", e.target.checked ? "true" : "")}
                  className="rounded border-gray-300 text-blue-600 focus:ring-blue-500"
                />
                New Today
              </label>
              <label className="flex items-center gap-2 text-sm text-gray-700 cursor-pointer">
                <input
                  type="checkbox"
                  checked={searchParams.get("fee_waived") === "true"}
                  onChange={(e) => updateFilter("fee_waived", e.target.checked ? "true" : "")}
                  className="rounded border-gray-300 text-blue-600 focus:ring-blue-500"
                />
                Fee Waived
              </label>
            </div>
          </div>
        </div>
      </aside>

      {/* Main Content */}
      <div className="flex-1 min-w-0">
        {/* Sort Bar */}
        <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-3 mb-6">
          <p className="text-sm text-gray-500">
            {loading
              ? "Loading..."
              : `${total.toLocaleString()} dog${total !== 1 ? "s" : ""} found`}
          </p>
          <div className="flex items-center gap-2">
            <label htmlFor="sort-select" className="text-sm text-gray-600">
              Sort by:
            </label>
            <select
              id="sort-select"
              value={sort}
              onChange={(e) => updateFilter("sort", e.target.value)}
              className="px-3 py-1.5 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            >
              {SORT_OPTIONS.map((option) => (
                <option key={option.value} value={option.value}>
                  {option.label}
                </option>
              ))}
            </select>
          </div>
        </div>

        {/* Error State */}
        {error && (
          <div className="mb-6 p-4 bg-red-50 border border-red-200 rounded-lg text-red-700 text-sm">
            {error}
            <button
              onClick={fetchDogs}
              className="ml-2 underline hover:no-underline"
            >
              Retry
            </button>
          </div>
        )}

        {/* Loading State */}
        {loading && (
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
            {Array.from({ length: 6 }).map((_, i) => (
              <div
                key={i}
                className="bg-white rounded-lg shadow-sm border border-gray-200 overflow-hidden animate-pulse"
              >
                <div className="aspect-square bg-gray-200" />
                <div className="p-4 space-y-2">
                  <div className="h-5 bg-gray-200 rounded w-3/4" />
                  <div className="h-4 bg-gray-200 rounded w-1/2" />
                  <div className="h-3 bg-gray-200 rounded w-1/3" />
                  <div className="h-8 bg-gray-900 rounded mt-3" />
                </div>
              </div>
            ))}
          </div>
        )}

        {/* Dog Grid */}
        {!loading && (
          <DogGrid
            dogs={dogs}
            urgencyHighlight
            emptyMessage={
              hasFilters
                ? "No dogs match your filters. Try adjusting your criteria."
                : "No dogs available yet. Check back soon!"
            }
          />
        )}

        {/* Pagination */}
        {!loading && totalPages > 1 && (
          <Pagination
            currentPage={currentPage}
            totalPages={totalPages}
            onPageChange={handlePageChange}
          />
        )}
      </div>
    </div>
  );
}
