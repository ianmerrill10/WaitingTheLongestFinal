import Link from "next/link";
import type { Metadata } from "next";
import { createClient } from "@/lib/supabase/server";

export const metadata: Metadata = {
  title: "Dog Breeds Available for Adoption",
  description: "Browse adoptable dogs by breed. Find Labrador Retrievers, German Shepherds, Pit Bulls, and more at shelters across America.",
};

export default async function BreedsPage() {
  const supabase = await createClient();

  // Get all breeds with counts
  const { data: breeds } = await supabase
    .from("dogs")
    .select("breed_primary")
    .eq("is_available", true);

  // Count occurrences
  const breedCounts: Record<string, number> = {};
  (breeds || []).forEach((d) => {
    const b = d.breed_primary;
    if (b) breedCounts[b] = (breedCounts[b] || 0) + 1;
  });

  const sortedBreeds = Object.entries(breedCounts)
    .sort((a, b) => b[1] - a[1]);

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      <h1 className="text-3xl font-bold text-gray-900 mb-2">Browse by Breed</h1>
      <p className="text-gray-500 mb-8">
        {sortedBreeds.length} breeds available for adoption across the United States
      </p>

      <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-3">
        {sortedBreeds.map(([breed, count]) => (
          <Link
            key={breed}
            href={`/breeds/${encodeURIComponent(breed.toLowerCase().replace(/ /g, "-"))}`}
            className="flex items-center justify-between p-3 bg-white rounded-lg border border-gray-200 hover:border-blue-300 hover:shadow-sm transition"
          >
            <span className="font-medium text-gray-900 text-sm truncate">{breed}</span>
            <span className="text-xs text-gray-400 bg-gray-100 px-2 py-0.5 rounded-full flex-shrink-0 ml-2">
              {count.toLocaleString()}
            </span>
          </Link>
        ))}
      </div>
    </div>
  );
}
