import type { Metadata } from "next";
import { Suspense } from "react";
import DogListings from "@/components/dogs/DogListings";

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

export default function SearchPage() {
  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      <div className="mb-6">
        <h1 className="text-3xl md:text-4xl font-bold text-gray-900 mb-2">
          Search Dogs
        </h1>
        <p className="text-gray-600">
          Find your perfect match from shelters and rescues across America.
        </p>
      </div>
      <Suspense fallback={<div className="text-center py-12 text-gray-500">Loading...</div>}>
        <DogListings />
      </Suspense>
    </div>
  );
}
