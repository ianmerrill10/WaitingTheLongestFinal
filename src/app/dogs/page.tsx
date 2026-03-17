import type { Metadata } from "next";
import { Suspense } from "react";
import DogListings from "@/components/dogs/DogListings";

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

export default function DogsPage() {
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

      <Suspense
        fallback={
          <div className="text-center py-12 text-gray-500">
            Loading dog listings...
          </div>
        }
      >
        <DogListings />
      </Suspense>
    </div>
  );
}
