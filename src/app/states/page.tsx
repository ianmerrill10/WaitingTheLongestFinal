import type { Metadata } from "next";
import Link from "next/link";
import { createClient } from "@/lib/supabase/server";

export const metadata: Metadata = {
  title: "Browse by State - Adoptable Dogs Across America",
  description:
    "Find adoptable shelter dogs in your state. Browse all 50 states to see dogs waiting for homes and local shelters near you.",
  openGraph: {
    title: "Browse by State | WaitingTheLongest.com",
    description:
      "Find adoptable dogs and shelters in your state. All 50 states covered.",
  },
};

const US_STATES = [
  { code: "AL", name: "Alabama" },
  { code: "AK", name: "Alaska" },
  { code: "AZ", name: "Arizona" },
  { code: "AR", name: "Arkansas" },
  { code: "CA", name: "California" },
  { code: "CO", name: "Colorado" },
  { code: "CT", name: "Connecticut" },
  { code: "DE", name: "Delaware" },
  { code: "FL", name: "Florida" },
  { code: "GA", name: "Georgia" },
  { code: "HI", name: "Hawaii" },
  { code: "ID", name: "Idaho" },
  { code: "IL", name: "Illinois" },
  { code: "IN", name: "Indiana" },
  { code: "IA", name: "Iowa" },
  { code: "KS", name: "Kansas" },
  { code: "KY", name: "Kentucky" },
  { code: "LA", name: "Louisiana" },
  { code: "ME", name: "Maine" },
  { code: "MD", name: "Maryland" },
  { code: "MA", name: "Massachusetts" },
  { code: "MI", name: "Michigan" },
  { code: "MN", name: "Minnesota" },
  { code: "MS", name: "Mississippi" },
  { code: "MO", name: "Missouri" },
  { code: "MT", name: "Montana" },
  { code: "NE", name: "Nebraska" },
  { code: "NV", name: "Nevada" },
  { code: "NH", name: "New Hampshire" },
  { code: "NJ", name: "New Jersey" },
  { code: "NM", name: "New Mexico" },
  { code: "NY", name: "New York" },
  { code: "NC", name: "North Carolina" },
  { code: "ND", name: "North Dakota" },
  { code: "OH", name: "Ohio" },
  { code: "OK", name: "Oklahoma" },
  { code: "OR", name: "Oregon" },
  { code: "PA", name: "Pennsylvania" },
  { code: "RI", name: "Rhode Island" },
  { code: "SC", name: "South Carolina" },
  { code: "SD", name: "South Dakota" },
  { code: "TN", name: "Tennessee" },
  { code: "TX", name: "Texas" },
  { code: "UT", name: "Utah" },
  { code: "VT", name: "Vermont" },
  { code: "VA", name: "Virginia" },
  { code: "WA", name: "Washington" },
  { code: "WV", name: "West Virginia" },
  { code: "WI", name: "Wisconsin" },
  { code: "WY", name: "Wyoming" },
];

export default async function StatesPage() {
  const supabase = await createClient();

  // Fetch all shelters' state codes to count per state
  const { data: shelterRows } = await supabase
    .from("shelters")
    .select("state_code");

  // Build a map of state_code -> shelter count
  const shelterCountByState: Record<string, number> = {};
  let totalShelters = 0;
  if (shelterRows) {
    for (const row of shelterRows) {
      const sc = row.state_code?.toUpperCase();
      if (sc) {
        shelterCountByState[sc] = (shelterCountByState[sc] || 0) + 1;
        totalShelters++;
      }
    }
  }

  // Fetch total available dogs count
  const { count: totalDogs } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .eq("is_available", true);

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      {/* Page Header */}
      <div className="mb-10">
        <h1 className="text-3xl md:text-4xl font-bold text-gray-900 mb-2">
          Browse by State
        </h1>
        <p className="text-gray-600 text-lg max-w-2xl">
          Find adoptable dogs and shelters in your state. Select a state to see
          available dogs, local shelters, and urgent cases near you.
        </p>
      </div>

      {/* State Grid */}
      <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-5 gap-3">
        {US_STATES.map((state) => {
          const count = shelterCountByState[state.code] || 0;
          return (
            <Link
              key={state.code}
              href={`/states/${state.code.toLowerCase()}`}
              className="group bg-white rounded-lg border border-gray-200 p-4 hover:border-blue-400 hover:shadow-md transition"
            >
              <div className="flex items-center gap-3">
                <span className="text-2xl font-bold text-blue-600 group-hover:text-blue-700 transition">
                  {state.code}
                </span>
                <div className="min-w-0">
                  <p className="font-medium text-gray-900 text-sm truncate">
                    {state.name}
                  </p>
                  <p className="text-xs text-gray-400">
                    {count} {count === 1 ? "shelter" : "shelters"}
                  </p>
                </div>
              </div>
            </Link>
          );
        })}
      </div>

      {/* Summary */}
      <div className="mt-10 text-center py-8 bg-blue-50 rounded-xl border border-blue-200">
        <svg
          className="mx-auto h-10 w-10 text-blue-400 mb-3"
          fill="none"
          viewBox="0 0 24 24"
          stroke="currentColor"
        >
          <path
            strokeLinecap="round"
            strokeLinejoin="round"
            strokeWidth={1}
            d="M3.055 11H5a2 2 0 012 2v1a2 2 0 002 2 2 2 0 012 2v2.945M8 3.935V5.5A2.5 2.5 0 0010.5 8h.5a2 2 0 012 2 2 2 0 104 0 2 2 0 012-2h1.064M15 20.488V18a2 2 0 012-2h3.064M21 12a9 9 0 11-18 0 9 9 0 0118 0z"
          />
        </svg>
        <p className="text-blue-800 font-medium">
          {totalShelters.toLocaleString()} shelters across all 50 states
        </p>
        <p className="text-sm text-blue-600 mt-1">
          {(totalDogs ?? 0).toLocaleString()} dogs currently available for
          adoption
        </p>
      </div>
    </div>
  );
}
