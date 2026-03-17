import type { Metadata } from "next";
import Link from "next/link";
import { DOG_SIZES, DOG_SIZE_LABELS, DOG_AGES, DOG_AGE_LABELS } from "@/lib/constants";

export const metadata: Metadata = {
  title: "Browse Adoptable Dogs",
  description:
    "Browse adoptable shelter dogs across the United States sorted by how long they've been waiting. Filter by breed, size, age, and more.",
  openGraph: {
    title: "Browse Adoptable Dogs | WaitingTheLongest.com",
    description:
      "Find your perfect shelter dog. Browse thousands of adoptable dogs sorted by wait time.",
  },
};

const SORT_OPTIONS = [
  { value: "wait_time", label: "Longest Waiting" },
  { value: "urgency", label: "Most Urgent" },
  { value: "newest", label: "Newest" },
  { value: "name", label: "Name (A-Z)" },
] as const;

const PLACEHOLDER_DOGS = Array.from({ length: 6 }, (_, i) => ({
  id: `placeholder-${i}`,
  name: `Dog ${i + 1}`,
  breed: "Breed",
  size: "Medium",
  age: "Adult",
  shelter: "Shelter Name",
  location: "City, ST",
}));

export default function DogsPage({
  searchParams,
}: {
  searchParams: Promise<{ [key: string]: string | string[] | undefined }>;
}) {
  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      {/* Page Header */}
      <div className="mb-8">
        <h1 className="text-3xl md:text-4xl font-bold text-gray-900 mb-2">
          Browse Adoptable Dogs
        </h1>
        <p className="text-gray-600 text-lg">
          Every dog deserves a home. The longest waiting dogs are shown first.
        </p>
      </div>

      <div className="flex flex-col lg:flex-row gap-8">
        {/* Filter Sidebar */}
        <aside className="w-full lg:w-64 flex-shrink-0">
          <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-5 sticky top-20">
            <h2 className="font-bold text-lg text-gray-900 mb-4">Filters</h2>

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
                placeholder="Search breeds..."
                className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                disabled
              />
            </div>

            {/* Size Filter */}
            <div className="mb-5">
              <span className="block text-sm font-semibold text-gray-700 mb-1">
                Size
              </span>
              <div className="space-y-1">
                {DOG_SIZES.map((size) => (
                  <label key={size} className="flex items-center gap-2 text-sm text-gray-600">
                    <input type="checkbox" className="rounded border-gray-300" disabled />
                    {DOG_SIZE_LABELS[size]}
                  </label>
                ))}
              </div>
            </div>

            {/* Age Filter */}
            <div className="mb-5">
              <span className="block text-sm font-semibold text-gray-700 mb-1">
                Age
              </span>
              <div className="space-y-1">
                {DOG_AGES.map((age) => (
                  <label key={age} className="flex items-center gap-2 text-sm text-gray-600">
                    <input type="checkbox" className="rounded border-gray-300" disabled />
                    {DOG_AGE_LABELS[age]}
                  </label>
                ))}
              </div>
            </div>

            {/* Gender Filter */}
            <div className="mb-5">
              <span className="block text-sm font-semibold text-gray-700 mb-1">
                Gender
              </span>
              <div className="space-y-1">
                {["Male", "Female"].map((gender) => (
                  <label key={gender} className="flex items-center gap-2 text-sm text-gray-600">
                    <input type="checkbox" className="rounded border-gray-300" disabled />
                    {gender}
                  </label>
                ))}
              </div>
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
                className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                disabled
              >
                <option value="">All States</option>
              </select>
            </div>

            {/* Urgency Level */}
            <div className="mb-5">
              <span className="block text-sm font-semibold text-gray-700 mb-1">
                Urgency Level
              </span>
              <div className="space-y-1">
                {[
                  { value: "critical", label: "Critical", color: "text-red-600" },
                  { value: "high", label: "Urgent", color: "text-orange-600" },
                  { value: "medium", label: "At Risk", color: "text-yellow-600" },
                  { value: "normal", label: "Available", color: "text-green-600" },
                ].map((level) => (
                  <label
                    key={level.value}
                    className={`flex items-center gap-2 text-sm ${level.color}`}
                  >
                    <input type="checkbox" className="rounded border-gray-300" disabled />
                    {level.label}
                  </label>
                ))}
              </div>
            </div>

            <button
              className="w-full py-2 px-4 bg-gray-100 text-gray-500 rounded-md text-sm font-medium cursor-not-allowed"
              disabled
            >
              Apply Filters
            </button>
          </div>
        </aside>

        {/* Main Content */}
        <div className="flex-1 min-w-0">
          {/* Sort Bar */}
          <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-3 mb-6">
            <p className="text-sm text-gray-500">
              Connect Supabase to see dogs
            </p>
            <div className="flex items-center gap-2">
              <label htmlFor="sort-select" className="text-sm text-gray-600">
                Sort by:
              </label>
              <select
                id="sort-select"
                className="px-3 py-1.5 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                defaultValue="wait_time"
                disabled
              >
                {SORT_OPTIONS.map((option) => (
                  <option key={option.value} value={option.value}>
                    {option.label}
                  </option>
                ))}
              </select>
            </div>
          </div>

          {/* LED Counter Placeholder */}
          <div className="mb-6 p-4 bg-gray-900 rounded-lg text-center">
            <p className="text-gray-400 text-sm mb-2">
              Combined Wait Time Counter
            </p>
            <p className="text-gray-500 text-xs">
              LEDCounter will display here when dogs are loaded
            </p>
          </div>

          {/* Dog Cards Grid */}
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
            {PLACEHOLDER_DOGS.map((dog) => (
              <div
                key={dog.id}
                className="bg-white rounded-lg shadow-sm border border-gray-200 overflow-hidden hover:shadow-md transition"
              >
                {/* Photo Placeholder */}
                <div className="aspect-square bg-gray-200 flex items-center justify-center">
                  <svg
                    className="w-16 h-16 text-gray-400"
                    fill="none"
                    viewBox="0 0 24 24"
                    stroke="currentColor"
                  >
                    <path
                      strokeLinecap="round"
                      strokeLinejoin="round"
                      strokeWidth={1}
                      d="M4 16l4.586-4.586a2 2 0 012.828 0L16 16m-2-2l1.586-1.586a2 2 0 012.828 0L20 14m-6-6h.01M6 20h12a2 2 0 002-2V6a2 2 0 00-2-2H6a2 2 0 00-2 2v12a2 2 0 002 2z"
                    />
                  </svg>
                </div>
                {/* Card Content */}
                <div className="p-4">
                  <div className="flex items-start justify-between mb-1">
                    <h3 className="font-bold text-gray-900">{dog.name}</h3>
                    <span className="urgency-badge urgency-normal text-xs">
                      AVAILABLE
                    </span>
                  </div>
                  <p className="text-sm text-gray-600 mb-1">{dog.breed}</p>
                  <p className="text-xs text-gray-500 mb-2">
                    {dog.size} &middot; {dog.age}
                  </p>
                  <p className="text-xs text-gray-400">{dog.location}</p>
                  {/* LED Counter placeholder */}
                  <div className="mt-3 bg-gray-900 rounded px-2 py-1 text-center">
                    <span className="text-gray-500 text-xs font-mono">
                      -- : -- : -- : -- : -- : --
                    </span>
                  </div>
                </div>
              </div>
            ))}
          </div>

          {/* Empty State */}
          <div className="text-center py-12 mt-8 bg-white rounded-lg border border-dashed border-gray-300">
            <svg
              className="mx-auto h-12 w-12 text-gray-400"
              fill="none"
              viewBox="0 0 24 24"
              stroke="currentColor"
            >
              <path
                strokeLinecap="round"
                strokeLinejoin="round"
                strokeWidth={1}
                d="M20 13V6a2 2 0 00-2-2H6a2 2 0 00-2 2v7m16 0v5a2 2 0 01-2 2H6a2 2 0 01-2-2v-5m16 0h-2.586a1 1 0 00-.707.293l-2.414 2.414a1 1 0 01-.707.293h-3.172a1 1 0 01-.707-.293l-2.414-2.414A1 1 0 006.586 13H4"
              />
            </svg>
            <h3 className="mt-3 text-lg font-semibold text-gray-900">
              Connect Supabase to see dogs
            </h3>
            <p className="mt-1 text-sm text-gray-500 max-w-sm mx-auto">
              Once the Supabase database is connected, adoptable dogs from shelters
              across America will appear here with real-time LED wait time counters.
            </p>
          </div>

          {/* Pagination */}
          <nav
            className="flex items-center justify-center gap-1 mt-8"
            aria-label="Pagination"
          >
            <button
              className="px-3 py-2 text-sm text-gray-400 bg-white border border-gray-200 rounded-md cursor-not-allowed"
              disabled
            >
              Previous
            </button>
            <span className="px-4 py-2 text-sm font-medium text-white bg-blue-600 rounded-md">
              1
            </span>
            <button
              className="px-3 py-2 text-sm text-gray-400 bg-white border border-gray-200 rounded-md cursor-not-allowed"
              disabled
            >
              2
            </button>
            <button
              className="px-3 py-2 text-sm text-gray-400 bg-white border border-gray-200 rounded-md cursor-not-allowed"
              disabled
            >
              3
            </button>
            <span className="px-2 text-gray-400">...</span>
            <button
              className="px-3 py-2 text-sm text-gray-400 bg-white border border-gray-200 rounded-md cursor-not-allowed"
              disabled
            >
              Next
            </button>
          </nav>
        </div>
      </div>
    </div>
  );
}
