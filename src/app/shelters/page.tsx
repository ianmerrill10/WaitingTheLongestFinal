import type { Metadata } from "next";
import { Suspense } from "react";
import ShelterListings from "@/components/shelters/ShelterListings";

export const metadata: Metadata = {
  title: "Shelter Directory - Find Shelters Near You",
  description:
    "Browse 44,000+ animal shelters and rescue organizations across all 50 states. Search by name, city, zip code, or state. Filter by type, sort by dog count.",
  openGraph: {
    title: "Shelter Directory | WaitingTheLongest.com",
    description:
      "Find animal shelters and rescues across America. Browse 44,000+ organizations with real-time data.",
  },
};

export default function SheltersPage() {
  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      {/* Page Header */}
      <div className="mb-8">
        <h1 className="text-3xl md:text-4xl font-bold text-gray-900 mb-2">
          Shelter Directory
        </h1>
        <p className="text-gray-600 text-lg">
          Search and filter animal shelters and rescue organizations across all 50 states.
          Export data, compare shelters, and find where dogs need help most.
        </p>
      </div>

      <Suspense
        fallback={
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
            {Array.from({ length: 6 }).map((_, i) => (
              <div key={i} className="bg-white rounded-lg shadow-sm border border-gray-200 p-5 animate-pulse">
                <div className="h-5 bg-gray-200 rounded w-3/4 mb-2" />
                <div className="h-4 bg-gray-200 rounded w-1/2 mb-3" />
                <div className="h-9 bg-gray-200 rounded mt-4" />
              </div>
            ))}
          </div>
        }
      >
        <ShelterListings />
      </Suspense>
    </div>
  );
}
