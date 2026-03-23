"use client";

import { useEffect, useState, useCallback } from "react";
import { useRouter, useSearchParams } from "next/navigation";
import ShelterCard from "./ShelterCard";
import ShelterTable from "./ShelterTable";
import ShelterExport from "./ShelterExport";
import Pagination from "@/components/ui/Pagination";

const SORT_OPTIONS = [
  { value: "name_asc", label: "Name (A-Z)" },
  { value: "name_desc", label: "Name (Z-A)" },
  { value: "state", label: "State" },
  { value: "most_dogs", label: "Most Dogs" },
  { value: "most_urgent", label: "Most Urgent" },
  { value: "recently_updated", label: "Recently Updated" },
  { value: "recently_scraped", label: "Recently Scraped" },
] as const;

const SHELTER_TYPES = [
  { value: "municipal", label: "Municipal" },
  { value: "private", label: "Private" },
  { value: "rescue", label: "Rescue" },
  { value: "foster_network", label: "Foster Network" },
  { value: "humane_society", label: "Humane Society" },
  { value: "spca", label: "SPCA" },
];

const US_STATES = [
  "AL","AK","AZ","AR","CA","CO","CT","DE","FL","GA",
  "HI","ID","IL","IN","IA","KS","KY","LA","ME","MD",
  "MA","MI","MN","MS","MO","MT","NE","NV","NH","NJ",
  "NM","NY","NC","ND","OH","OK","OR","PA","RI","SC",
  "SD","TN","TX","UT","VT","VA","WA","WV","WI","WY",
];

interface Shelter {
  id: string;
  name: string;
  city: string;
  state_code: string;
  zip_code?: string | null;
  shelter_type?: string | null;
  is_kill_shelter?: boolean;
  accepts_rescue_pulls?: boolean;
  is_verified?: boolean;
  phone?: string | null;
  email?: string | null;
  website?: string | null;
  available_dog_count?: number;
  urgent_dog_count?: number;
  website_platform?: string | null;
  last_scraped_at?: string | null;
  ein?: string | null;
}

interface SheltersResponse {
  shelters: Shelter[];
  total: number;
  page: number;
  limit: number;
  totalPages: number;
}

export default function ShelterListings() {
  const router = useRouter();
  const searchParams = useSearchParams();

  const [shelters, setShelters] = useState<Shelter[]>([]);
  const [total, setTotal] = useState(0);
  const [totalPages, setTotalPages] = useState(0);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [view, setView] = useState<"grid" | "table">(
    (searchParams.get("view") as "grid" | "table") || "grid"
  );

  // Read filters from URL
  const currentPage = parseInt(searchParams.get("page") || "1");
  const sort = searchParams.get("sort") || "name_asc";
  const search = searchParams.get("q") || "";
  const city = searchParams.get("city") || "";
  const zip = searchParams.get("zip") || "";
  const state = searchParams.get("state") || "";
  const type = searchParams.get("type") || "";
  const killShelter = searchParams.get("kill_shelter") || "";
  const rescuePulls = searchParams.get("rescue_pulls") || "";
  const verified = searchParams.get("verified") || "";
  const hasDogs = searchParams.get("has_dogs") || "";

  const fetchShelters = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const params = new URLSearchParams();
      params.set("page", String(currentPage));
      params.set("sort", sort);
      if (search) params.set("q", search);
      if (city) params.set("city", city);
      if (zip) params.set("zip", zip);
      if (state) params.set("state", state);
      if (type) params.set("type", type);
      if (killShelter) params.set("kill_shelter", killShelter);
      if (rescuePulls) params.set("rescue_pulls", rescuePulls);
      if (verified) params.set("verified", verified);
      if (hasDogs) params.set("has_dogs", hasDogs);

      const res = await fetch(`/api/shelters?${params.toString()}`);
      if (!res.ok) throw new Error("Failed to fetch shelters");
      const data: SheltersResponse = await res.json();
      setShelters(data.shelters);
      setTotal(data.total);
      setTotalPages(data.totalPages);
    } catch (err) {
      setError(err instanceof Error ? err.message : "Failed to load shelters");
    } finally {
      setLoading(false);
    }
  }, [currentPage, sort, search, city, zip, state, type, killShelter, rescuePulls, verified, hasDogs]);

  useEffect(() => {
    fetchShelters();
  }, [fetchShelters]);

  function updateFilter(key: string, value: string) {
    const params = new URLSearchParams(searchParams.toString());
    if (value) {
      params.set(key, value);
    } else {
      params.delete(key);
    }
    params.set("page", "1");
    router.push(`/shelters?${params.toString()}`);
  }

  function clearFilters() {
    router.push("/shelters");
  }

  function handlePageChange(page: number) {
    const params = new URLSearchParams(searchParams.toString());
    params.set("page", String(page));
    router.push(`/shelters?${params.toString()}`);
  }

  function handleViewChange(v: "grid" | "table") {
    setView(v);
    const params = new URLSearchParams(searchParams.toString());
    params.set("view", v);
    router.push(`/shelters?${params.toString()}`);
  }

  const hasFilters = search || city || zip || state || type || killShelter || rescuePulls || verified || hasDogs;

  // Build filter params string for export
  const filterParams = new URLSearchParams();
  if (search) filterParams.set("q", search);
  if (city) filterParams.set("city", city);
  if (zip) filterParams.set("zip", zip);
  if (state) filterParams.set("state", state);
  if (type) filterParams.set("type", type);
  if (killShelter) filterParams.set("kill_shelter", killShelter);
  if (rescuePulls) filterParams.set("rescue_pulls", rescuePulls);
  if (verified) filterParams.set("verified", verified);
  if (hasDogs) filterParams.set("has_dogs", hasDogs);

  return (
    <div className="flex flex-col lg:flex-row gap-8">
      {/* Filter Sidebar */}
      <aside className="w-full lg:w-64 flex-shrink-0">
        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-5 sticky top-20">
          <div className="flex items-center justify-between mb-4">
            <h2 className="font-bold text-lg text-gray-900">Filters</h2>
            {hasFilters && (
              <button onClick={clearFilters} className="text-xs text-blue-600 hover:text-blue-800">
                Clear all
              </button>
            )}
          </div>

          {/* Search */}
          <FilterSection label="Search Name">
            <input
              type="text"
              placeholder="Shelter name..."
              value={search}
              onChange={(e) => updateFilter("q", e.target.value)}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            />
          </FilterSection>

          {/* City */}
          <FilterSection label="City">
            <input
              type="text"
              placeholder="City name..."
              value={city}
              onChange={(e) => updateFilter("city", e.target.value)}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            />
          </FilterSection>

          {/* Zip Code */}
          <FilterSection label="Zip Code">
            <input
              type="text"
              placeholder="e.g. 90210"
              value={zip}
              onChange={(e) => updateFilter("zip", e.target.value)}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            />
          </FilterSection>

          {/* State */}
          <FilterSection label="State">
            <select
              value={state}
              onChange={(e) => updateFilter("state", e.target.value)}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            >
              <option value="">All States</option>
              {US_STATES.map((code) => (
                <option key={code} value={code}>{code}</option>
              ))}
            </select>
          </FilterSection>

          {/* Shelter Type */}
          <FilterSection label="Type">
            <select
              value={type}
              onChange={(e) => updateFilter("type", e.target.value)}
              className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            >
              <option value="">All Types</option>
              {SHELTER_TYPES.map((t) => (
                <option key={t.value} value={t.value}>{t.label}</option>
              ))}
            </select>
          </FilterSection>

          {/* Toggle Filters */}
          <FilterSection label="Attributes">
            <div className="space-y-2">
              <ToggleFilter
                label="Kill Shelters Only"
                checked={killShelter === "true"}
                onChange={(v) => updateFilter("kill_shelter", v ? "true" : "")}
              />
              <ToggleFilter
                label="Accepts Rescue Pulls"
                checked={rescuePulls === "true"}
                onChange={(v) => updateFilter("rescue_pulls", v ? "true" : "")}
              />
              <ToggleFilter
                label="Verified Only"
                checked={verified === "true"}
                onChange={(v) => updateFilter("verified", v ? "true" : "")}
              />
              <ToggleFilter
                label="Has Available Dogs"
                checked={hasDogs === "true"}
                onChange={(v) => updateFilter("has_dogs", v ? "true" : "")}
              />
            </div>
          </FilterSection>
        </div>
      </aside>

      {/* Main Content */}
      <div className="flex-1 min-w-0">
        {/* Top Bar */}
        <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-3 mb-6">
          <p className="text-sm text-gray-500">
            {loading ? "Loading..." : `${total.toLocaleString()} shelter${total !== 1 ? "s" : ""} found`}
          </p>
          <div className="flex items-center gap-3 flex-wrap">
            {/* Export */}
            <ShelterExport filterParams={filterParams.toString()} total={total} />

            {/* View Toggle */}
            <div className="flex items-center border border-gray-300 rounded-md overflow-hidden">
              <button
                onClick={() => handleViewChange("grid")}
                className={`px-2.5 py-1.5 text-xs ${view === "grid" ? "bg-blue-600 text-white" : "bg-white text-gray-600 hover:bg-gray-50"}`}
                title="Grid view"
              >
                <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                  <path strokeLinecap="round" strokeLinejoin="round" d="M4 6a2 2 0 012-2h2a2 2 0 012 2v2a2 2 0 01-2 2H6a2 2 0 01-2-2V6zm10 0a2 2 0 012-2h2a2 2 0 012 2v2a2 2 0 01-2 2h-2a2 2 0 01-2-2V6zM4 16a2 2 0 012-2h2a2 2 0 012 2v2a2 2 0 01-2 2H6a2 2 0 01-2-2v-2zm10 0a2 2 0 012-2h2a2 2 0 012 2v2a2 2 0 01-2 2h-2a2 2 0 01-2-2v-2z" />
                </svg>
              </button>
              <button
                onClick={() => handleViewChange("table")}
                className={`px-2.5 py-1.5 text-xs ${view === "table" ? "bg-blue-600 text-white" : "bg-white text-gray-600 hover:bg-gray-50"}`}
                title="Table view"
              >
                <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                  <path strokeLinecap="round" strokeLinejoin="round" d="M4 6h16M4 10h16M4 14h16M4 18h16" />
                </svg>
              </button>
            </div>

            {/* Sort */}
            <div className="flex items-center gap-2">
              <label htmlFor="shelter-sort" className="text-sm text-gray-600">Sort:</label>
              <select
                id="shelter-sort"
                value={sort}
                onChange={(e) => updateFilter("sort", e.target.value)}
                className="px-3 py-1.5 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
              >
                {SORT_OPTIONS.map((option) => (
                  <option key={option.value} value={option.value}>{option.label}</option>
                ))}
              </select>
            </div>
          </div>
        </div>

        {/* Error */}
        {error && (
          <div className="mb-6 p-4 bg-red-50 border border-red-200 rounded-lg text-red-700 text-sm">
            {error}
            <button onClick={fetchShelters} className="ml-2 underline hover:no-underline">Retry</button>
          </div>
        )}

        {/* Loading */}
        {loading && (
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
            {Array.from({ length: 6 }).map((_, i) => (
              <div key={i} className="bg-white rounded-lg shadow-sm border border-gray-200 p-5 animate-pulse">
                <div className="h-5 bg-gray-200 rounded w-3/4 mb-2" />
                <div className="h-4 bg-gray-200 rounded w-1/2 mb-3" />
                <div className="flex gap-2 mb-3">
                  <div className="h-5 bg-gray-200 rounded w-16" />
                  <div className="h-5 bg-gray-200 rounded w-20" />
                </div>
                <div className="h-4 bg-gray-200 rounded w-1/3 mb-4" />
                <div className="h-9 bg-gray-200 rounded" />
              </div>
            ))}
          </div>
        )}

        {/* Content */}
        {!loading && view === "grid" && (
          shelters.length > 0 ? (
            <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
              {shelters.map((shelter) => (
                <ShelterCard key={shelter.id} shelter={shelter} />
              ))}
            </div>
          ) : (
            <div className="text-center py-12 bg-gray-50 rounded-lg border border-dashed border-gray-300">
              <svg className="mx-auto h-10 w-10 text-gray-400 mb-3" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1} d="M19 21V5a2 2 0 00-2-2H7a2 2 0 00-2 2v16m14 0h2m-2 0h-5m-9 0H3m2 0h5M9 7h1m-1 4h1m4-4h1m-1 4h1m-5 10v-5a1 1 0 011-1h2a1 1 0 011 1v5m-4 0h4" />
              </svg>
              <p className="text-gray-600 font-medium">No shelters found</p>
              <p className="text-sm text-gray-500 mt-1">
                {hasFilters ? "Try adjusting your filters." : "Check back soon as new shelters are being added."}
              </p>
            </div>
          )
        )}

        {!loading && view === "table" && (
          <ShelterTable
            shelters={shelters}
            sort={sort}
            onSort={(s) => updateFilter("sort", s)}
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

function FilterSection({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <div className="mb-5">
      <label className="block text-sm font-semibold text-gray-700 mb-1">{label}</label>
      {children}
    </div>
  );
}

function ToggleFilter({ label, checked, onChange }: { label: string; checked: boolean; onChange: (v: boolean) => void }) {
  return (
    <label className="flex items-center gap-2 text-sm text-gray-700 cursor-pointer">
      <input
        type="checkbox"
        checked={checked}
        onChange={(e) => onChange(e.target.checked)}
        className="rounded border-gray-300 text-blue-600 focus:ring-blue-500"
      />
      {label}
    </label>
  );
}
