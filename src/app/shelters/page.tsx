import type { Metadata } from "next";
import Link from "next/link";
import { createClient } from "@/lib/supabase/server";
import ShelterSearch from "@/components/shelters/ShelterSearch";

export const metadata: Metadata = {
  title: "Shelter Directory - Find Shelters Near You",
  description:
    "Browse animal shelters and rescue organizations across all 50 states. Find shelters near you and see their available dogs.",
  openGraph: {
    title: "Shelter Directory | WaitingTheLongest.com",
    description:
      "Find animal shelters and rescues across America. Browse by name or state.",
  },
};

const US_STATES = [
  { code: "AL", name: "Alabama" }, { code: "AK", name: "Alaska" },
  { code: "AZ", name: "Arizona" }, { code: "AR", name: "Arkansas" },
  { code: "CA", name: "California" }, { code: "CO", name: "Colorado" },
  { code: "CT", name: "Connecticut" }, { code: "DE", name: "Delaware" },
  { code: "FL", name: "Florida" }, { code: "GA", name: "Georgia" },
  { code: "HI", name: "Hawaii" }, { code: "ID", name: "Idaho" },
  { code: "IL", name: "Illinois" }, { code: "IN", name: "Indiana" },
  { code: "IA", name: "Iowa" }, { code: "KS", name: "Kansas" },
  { code: "KY", name: "Kentucky" }, { code: "LA", name: "Louisiana" },
  { code: "ME", name: "Maine" }, { code: "MD", name: "Maryland" },
  { code: "MA", name: "Massachusetts" }, { code: "MI", name: "Michigan" },
  { code: "MN", name: "Minnesota" }, { code: "MS", name: "Mississippi" },
  { code: "MO", name: "Missouri" }, { code: "MT", name: "Montana" },
  { code: "NE", name: "Nebraska" }, { code: "NV", name: "Nevada" },
  { code: "NH", name: "New Hampshire" }, { code: "NJ", name: "New Jersey" },
  { code: "NM", name: "New Mexico" }, { code: "NY", name: "New York" },
  { code: "NC", name: "North Carolina" }, { code: "ND", name: "North Dakota" },
  { code: "OH", name: "Ohio" }, { code: "OK", name: "Oklahoma" },
  { code: "OR", name: "Oregon" }, { code: "PA", name: "Pennsylvania" },
  { code: "RI", name: "Rhode Island" }, { code: "SC", name: "South Carolina" },
  { code: "SD", name: "South Dakota" }, { code: "TN", name: "Tennessee" },
  { code: "TX", name: "Texas" }, { code: "UT", name: "Utah" },
  { code: "VT", name: "Vermont" }, { code: "VA", name: "Virginia" },
  { code: "WA", name: "Washington" }, { code: "WV", name: "West Virginia" },
  { code: "WI", name: "Wisconsin" }, { code: "WY", name: "Wyoming" },
];

const PAGE_SIZE = 24;

export default async function SheltersPage({
  searchParams,
}: {
  searchParams: { q?: string; state?: string; page?: string };
}) {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  let shelters: any[] = [];
  let totalCount = 0;
  const currentPage = Math.max(1, parseInt(searchParams.page || "1", 10));
  const query = searchParams.q || "";
  const stateFilter = searchParams.state || "";

  try {
    const supabase = await createClient();

    let shelterQuery = supabase
      .from("shelters")
      .select("*", { count: "exact" });

    if (query) {
      shelterQuery = shelterQuery.ilike("name", `%${query}%`);
    }

    if (stateFilter) {
      shelterQuery = shelterQuery.eq("state_code", stateFilter.toUpperCase());
    }

    const { data, count } = await shelterQuery
      .order("name")
      .range((currentPage - 1) * PAGE_SIZE, currentPage * PAGE_SIZE - 1);

    shelters = data || [];
    totalCount = count || 0;
  } catch {
    // Supabase not configured yet
  }

  const totalPages = Math.ceil(totalCount / PAGE_SIZE);

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      {/* Page Header */}
      <div className="mb-8">
        <h1 className="text-3xl md:text-4xl font-bold text-gray-900 mb-2">
          Shelter Directory
        </h1>
        <p className="text-gray-600 text-lg">
          Find animal shelters and rescue organizations across the United States.
          {totalCount > 0 && (
            <span className="text-gray-500">
              {" "}
              Currently tracking{" "}
              <span className="font-semibold text-gray-700">
                {totalCount.toLocaleString()}
              </span>{" "}
              organizations.
            </span>
          )}
        </p>
      </div>

      {/* Search Bar */}
      <ShelterSearch initialQuery={query} initialState={stateFilter} />

      {/* Active Filters */}
      {(query || stateFilter) && (
        <div className="mb-6 flex items-center gap-2 flex-wrap">
          <span className="text-sm text-gray-500">Filters:</span>
          {query && (
            <span className="px-3 py-1 bg-blue-50 text-blue-700 rounded-full text-sm">
              &quot;{query}&quot;
            </span>
          )}
          {stateFilter && (
            <span className="px-3 py-1 bg-green-50 text-green-700 rounded-full text-sm">
              {stateFilter.toUpperCase()}
            </span>
          )}
          <Link
            href="/shelters"
            className="text-sm text-red-500 hover:text-red-700"
          >
            Clear all
          </Link>
        </div>
      )}

      {/* Browse by State */}
      <section className="mb-10">
        <h2 className="text-xl font-bold text-gray-900 mb-4">
          Browse by State
        </h2>
        <div className="flex flex-wrap gap-2">
          {US_STATES.map((state) => (
            <Link
              key={state.code}
              href={`/shelters?state=${state.code}`}
              className={`px-3 py-1.5 border rounded-full text-sm font-medium transition ${
                stateFilter.toUpperCase() === state.code
                  ? "bg-blue-600 text-white border-blue-600"
                  : "bg-white border-gray-200 text-gray-700 hover:border-blue-400 hover:text-blue-600 hover:bg-blue-50"
              }`}
            >
              {state.code}
            </Link>
          ))}
        </div>
      </section>

      {/* Shelter Grid */}
      <section>
        <div className="flex items-center justify-between mb-6">
          <h2 className="text-xl font-bold text-gray-900">
            {stateFilter ? `Shelters in ${stateFilter.toUpperCase()}` : query ? `Results for "${query}"` : "All Shelters"}
          </h2>
          {shelters.length > 0 && (
            <p className="text-sm text-gray-500">
              Showing {(currentPage - 1) * PAGE_SIZE + 1}-
              {Math.min(currentPage * PAGE_SIZE, totalCount)} of{" "}
              {totalCount.toLocaleString()}
            </p>
          )}
        </div>

        {shelters.length > 0 ? (
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
            {shelters.map((shelter) => (
              <div
                key={shelter.id}
                className="bg-white rounded-lg border border-gray-200 p-5 hover:shadow-md transition"
              >
                <div className="flex items-start justify-between mb-3">
                  <div className="min-w-0">
                    <h3 className="font-bold text-gray-900 truncate">
                      {shelter.name}
                    </h3>
                    <p className="text-sm text-gray-500">
                      {shelter.city}, {shelter.state_code}
                    </p>
                  </div>
                  {shelter.shelter_type && (
                    <span className="flex-shrink-0 px-2 py-0.5 text-xs bg-gray-100 text-gray-600 rounded-full capitalize">
                      {shelter.shelter_type.replace(/_/g, " ")}
                    </span>
                  )}
                </div>
                <div className="flex flex-wrap items-center gap-2 text-xs mb-4">
                  {shelter.is_kill_shelter && (
                    <span className="px-2 py-0.5 bg-red-50 text-red-600 rounded-full">
                      Kill Shelter
                    </span>
                  )}
                  {shelter.accepts_rescue_pulls && (
                    <span className="px-2 py-0.5 bg-green-50 text-green-600 rounded-full">
                      Accepts Rescue Pulls
                    </span>
                  )}
                  {shelter.phone && (
                    <span className="text-gray-500">{shelter.phone}</span>
                  )}
                </div>
                <Link
                  href={`/shelters/${shelter.id}`}
                  className="block w-full text-center py-2 px-4 bg-gray-50 hover:bg-gray-100 text-gray-700 font-medium rounded-md text-sm transition border border-gray-200"
                >
                  View Shelter
                </Link>
              </div>
            ))}
          </div>
        ) : (
          <div className="text-center py-12 bg-gray-50 rounded-lg border border-dashed border-gray-300">
            <svg
              className="mx-auto h-10 w-10 text-gray-400 mb-3"
              fill="none"
              viewBox="0 0 24 24"
              stroke="currentColor"
            >
              <path
                strokeLinecap="round"
                strokeLinejoin="round"
                strokeWidth={1}
                d="M19 21V5a2 2 0 00-2-2H7a2 2 0 00-2 2v16m14 0h2m-2 0h-5m-9 0H3m2 0h5M9 7h1m-1 4h1m4-4h1m-1 4h1m-5 10v-5a1 1 0 011-1h2a1 1 0 011 1v5m-4 0h4"
              />
            </svg>
            <p className="text-gray-600 font-medium">No shelters found</p>
            <p className="text-sm text-gray-500 mt-1">
              {query ? `No results for "${query}". Try a different search term.` : "Check back soon as new shelters are being added regularly."}
            </p>
          </div>
        )}

        {/* Pagination */}
        {totalPages > 1 && (
          <div className="flex items-center justify-center gap-2 mt-8">
            {currentPage > 1 && (
              <Link
                href={`/shelters?${new URLSearchParams({ ...(query && { q: query }), ...(stateFilter && { state: stateFilter }), page: String(currentPage - 1) }).toString()}`}
                className="px-4 py-2 border border-gray-300 rounded-lg text-sm hover:bg-gray-50"
              >
                Previous
              </Link>
            )}
            <span className="text-sm text-gray-600">
              Page {currentPage} of {totalPages}
            </span>
            {currentPage < totalPages && (
              <Link
                href={`/shelters?${new URLSearchParams({ ...(query && { q: query }), ...(stateFilter && { state: stateFilter }), page: String(currentPage + 1) }).toString()}`}
                className="px-4 py-2 border border-gray-300 rounded-lg text-sm hover:bg-gray-50"
              >
                Next
              </Link>
            )}
          </div>
        )}
      </section>
    </div>
  );
}
