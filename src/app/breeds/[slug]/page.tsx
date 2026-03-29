import type { Metadata } from "next";
import Link from "next/link";
import { createAdminClient } from "@/lib/supabase/admin";
import DogGrid from "@/components/dogs/DogGrid";

function slugToBreed(slug: string): string {
  return decodeURIComponent(slug).replace(/-/g, " ").replace(/\b\w/g, l => l.toUpperCase());
}

export async function generateMetadata({
  params,
}: {
  params: Promise<{ slug: string }>;
}): Promise<Metadata> {
  const { slug } = await params;
  const breed = slugToBreed(slug);
  return {
    title: `Adopt a ${breed} | Adoptable ${breed} Dogs`,
    description: `Find ${breed} dogs available for adoption at shelters across the United States. View profiles, see wait times, and help save a life.`,
  };
}

export default async function BreedPage({
  params,
}: {
  params: Promise<{ slug: string }>;
}) {
  const { slug } = await params;
  const breed = slugToBreed(slug);

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  let dogs: any[] = [];
  let totalCount = 0;

  try {
    const supabase = createAdminClient();

    // Separate count query from data query — shelters!inner + count can return wrong counts
    const [dogsRes, countRes] = await Promise.all([
      supabase
        .from("dogs")
        .select("*, shelters(name, city, state_code)")
        .eq("is_available", true)
        .ilike("breed_primary", breed)
        .order("intake_date", { ascending: true })
        .limit(24),
      supabase
        .from("dogs")
        .select("id", { count: "exact", head: true })
        .eq("is_available", true)
        .ilike("breed_primary", breed),
    ]);

    dogs = dogsRes.data || [];
    totalCount = countRes.count || 0;
  } catch (err) {
    console.error("[BreedPage] Failed to fetch data:", err);
  }

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      {/* Breadcrumb */}
      <nav className="mb-6 text-sm text-gray-500">
        <ol className="flex items-center gap-2">
          <li><Link href="/" className="hover:text-gray-700">Home</Link></li>
          <li>/</li>
          <li><Link href="/breeds" className="hover:text-gray-700">Breeds</Link></li>
          <li>/</li>
          <li className="text-gray-900 font-medium">{breed}</li>
        </ol>
      </nav>

      <div className="mb-8">
        <h1 className="text-3xl font-bold text-gray-900 mb-2">
          {breed} Dogs Available for Adoption
        </h1>
        <p className="text-gray-500">
          {totalCount.toLocaleString()} {breed} dog{totalCount !== 1 ? "s" : ""} waiting for a forever home
        </p>
      </div>

      {dogs && dogs.length > 0 ? (
        <DogGrid dogs={dogs} />
      ) : (
        <div className="text-center py-16">
          <p className="text-gray-500 text-lg mb-4">No {breed} dogs currently available.</p>
          <Link href="/breeds" className="text-blue-600 hover:underline">Browse all breeds</Link>
        </div>
      )}

      {totalCount > 24 && (
        <div className="mt-8 text-center">
          <Link
            href={`/dogs?breed=${encodeURIComponent(breed)}`}
            className="inline-block px-6 py-3 bg-blue-600 text-white font-medium rounded-lg hover:bg-blue-700 transition"
          >
            View All {totalCount.toLocaleString()} {breed} Dogs
          </Link>
        </div>
      )}
    </div>
  );
}
