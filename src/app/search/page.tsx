import type { Metadata } from "next";
import Link from "next/link";
import { DOG_SIZES, DOG_SIZE_LABELS, DOG_AGES, DOG_AGE_LABELS } from "@/lib/constants";

export const metadata: Metadata = {
  title: "Search Dogs",
  description:
    "Search for adoptable shelter dogs by name, breed, location, and more. Find your perfect match.",
  openGraph: {
    title: "Search Dogs | WaitingTheLongest.com",
    description:
      "Search thousands of adoptable dogs from shelters across America.",
  },
};

export default function SearchPage({
  searchParams,
}: {
  searchParams: Promise<{ [key: string]: string | string[] | undefined }>;
}) {
  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      {/* Search Header */}
      <div className="mb-8">
        <h1 className="text-3xl md:text-4xl font-bold text-gray-900 mb-4">
          Search Dogs
        </h1>

        {/* Search Input */}
        <form action="/search" method="GET" className="max-w-2xl">
          <div className="flex gap-2">
            <div className="relative flex-1">
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
                type="text"
                name="q"
                placeholder="Search by breed, name, or location..."
                className="w-full pl-10 pr-4 py-3 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                disabled
              />
            </div>
            <button
              type="submit"
              className="px-6 py-3 bg-blue-600 text-white font-semibold rounded-lg hover:bg-blue-700 transition cursor-not-allowed"
              disabled
            >
              Search
            </button>
          </div>
        </form>
      </div>

      <div className="flex flex-col lg:flex-row gap-8">
        {/* Filters Sidebar */}
        <aside className="w-full lg:w-56 flex-shrink-0">
          <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-5">
            <h2 className="font-bold text-gray-900 mb-4">Refine Results</h2>

            {/* Size */}
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

            {/* Age */}
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

            {/* Gender */}
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

            {/* Good With */}
            <div className="mb-5">
              <span className="block text-sm font-semibold text-gray-700 mb-1">
                Good With
              </span>
              <div className="space-y-1">
                {["Kids", "Dogs", "Cats"].map((item) => (
                  <label key={item} className="flex items-center gap-2 text-sm text-gray-600">
                    <input type="checkbox" className="rounded border-gray-300" disabled />
                    {item}
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

        {/* Results Area */}
        <div className="flex-1 min-w-0">
          {/* Results Count */}
          <div className="flex items-center justify-between mb-6">
            <p className="text-sm text-gray-500">
              Enter a search term to find dogs
            </p>
          </div>

          {/* Results Grid Placeholder */}
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
            {Array.from({ length: 6 }).map((_, i) => (
              <div
                key={i}
                className="bg-white rounded-lg border border-gray-200 overflow-hidden opacity-50"
              >
                <div className="aspect-square bg-gray-200 flex items-center justify-center">
                  <svg
                    className="w-12 h-12 text-gray-300"
                    fill="none"
                    viewBox="0 0 24 24"
                    stroke="currentColor"
                  >
                    <path
                      strokeLinecap="round"
                      strokeLinejoin="round"
                      strokeWidth={1}
                      d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z"
                    />
                  </svg>
                </div>
                <div className="p-4">
                  <div className="h-4 bg-gray-200 rounded w-2/3 mb-2" />
                  <div className="h-3 bg-gray-200 rounded w-1/2 mb-2" />
                  <div className="h-3 bg-gray-200 rounded w-1/3" />
                </div>
              </div>
            ))}
          </div>

          {/* Empty State */}
          <div className="text-center py-16 mt-6">
            <svg
              className="mx-auto h-16 w-16 text-gray-300 mb-4"
              fill="none"
              viewBox="0 0 24 24"
              stroke="currentColor"
            >
              <path
                strokeLinecap="round"
                strokeLinejoin="round"
                strokeWidth={1}
                d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z"
              />
            </svg>
            <h3 className="text-xl font-semibold text-gray-900 mb-2">
              Search for Adoptable Dogs
            </h3>
            <p className="text-gray-500 max-w-md mx-auto mb-6">
              Connect Supabase to enable search. You will be able to search by
              breed, name, location, and more.
            </p>
            <div className="flex flex-col sm:flex-row gap-3 justify-center">
              <Link
                href="/dogs"
                className="px-6 py-2.5 bg-blue-600 text-white font-medium rounded-lg hover:bg-blue-700 transition"
              >
                Browse All Dogs
              </Link>
              <Link
                href="/urgent"
                className="px-6 py-2.5 bg-red-600 text-white font-medium rounded-lg hover:bg-red-700 transition"
              >
                View Urgent Dogs
              </Link>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
