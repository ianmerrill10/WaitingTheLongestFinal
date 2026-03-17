import type { Metadata } from "next";
import Link from "next/link";

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

const PLACEHOLDER_SHELTERS = Array.from({ length: 6 }, (_, i) => ({
  id: `shelter-${i}`,
  name: `Shelter ${i + 1}`,
  city: "City",
  state_code: "ST",
  shelter_type: "municipal" as const,
  available_dogs: 0,
  critical_dogs: 0,
}));

export default function SheltersPage() {
  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      {/* Page Header */}
      <div className="mb-8">
        <h1 className="text-3xl md:text-4xl font-bold text-gray-900 mb-2">
          Shelter Directory
        </h1>
        <p className="text-gray-600 text-lg">
          Find animal shelters and rescue organizations across the United States.
        </p>
      </div>

      {/* Search Bar */}
      <div className="mb-8">
        <div className="max-w-xl">
          <label htmlFor="shelter-search" className="sr-only">
            Search shelters
          </label>
          <div className="relative">
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
              id="shelter-search"
              type="text"
              placeholder="Search by shelter name..."
              className="w-full pl-10 pr-4 py-3 border border-gray-300 rounded-lg text-sm focus:outline-none focus:ring-2 focus:ring-blue-500 focus:border-transparent"
              disabled
            />
          </div>
          <p className="text-xs text-gray-400 mt-1.5">
            Search will be available once Supabase is connected
          </p>
        </div>
      </div>

      {/* Browse by State */}
      <section className="mb-10">
        <h2 className="text-xl font-bold text-gray-900 mb-4">
          Browse by State
        </h2>
        <div className="flex flex-wrap gap-2">
          {US_STATES.map((state) => (
            <Link
              key={state.code}
              href={`/states/${state.code.toLowerCase()}`}
              className="px-3 py-1.5 bg-white border border-gray-200 rounded-full text-sm font-medium text-gray-700 hover:border-blue-400 hover:text-blue-600 hover:bg-blue-50 transition"
            >
              {state.code}
            </Link>
          ))}
        </div>
      </section>

      {/* Shelter Grid */}
      <section>
        <div className="flex items-center justify-between mb-6">
          <h2 className="text-xl font-bold text-gray-900">All Shelters</h2>
          <p className="text-sm text-gray-500">
            Connect Supabase to see shelters
          </p>
        </div>

        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
          {PLACEHOLDER_SHELTERS.map((shelter) => (
            <div
              key={shelter.id}
              className="bg-white rounded-lg border border-gray-200 p-5 hover:shadow-md transition"
            >
              <div className="flex items-start justify-between mb-3">
                <div>
                  <h3 className="font-bold text-gray-900">{shelter.name}</h3>
                  <p className="text-sm text-gray-500">
                    {shelter.city}, {shelter.state_code}
                  </p>
                </div>
                <span className="px-2 py-0.5 text-xs bg-gray-100 text-gray-600 rounded-full capitalize">
                  {shelter.shelter_type}
                </span>
              </div>
              <div className="flex items-center gap-4 text-sm text-gray-600 mb-4">
                <div className="flex items-center gap-1">
                  <svg
                    className="w-4 h-4 text-gray-400"
                    fill="none"
                    viewBox="0 0 24 24"
                    stroke="currentColor"
                  >
                    <path
                      strokeLinecap="round"
                      strokeLinejoin="round"
                      strokeWidth={2}
                      d="M19 11H5m14 0a2 2 0 012 2v6a2 2 0 01-2 2H5a2 2 0 01-2-2v-6a2 2 0 012-2m14 0V9a2 2 0 00-2-2M5 11V9a2 2 0 012-2m0 0V5a2 2 0 012-2h6a2 2 0 012 2v2M7 7h10"
                    />
                  </svg>
                  <span>-- dogs</span>
                </div>
                <div className="flex items-center gap-1">
                  <span className="w-2 h-2 bg-red-500 rounded-full" />
                  <span className="text-red-600">-- urgent</span>
                </div>
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

        {/* Empty State */}
        <div className="text-center py-12 mt-6 bg-gray-50 rounded-lg border border-dashed border-gray-300">
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
          <p className="text-gray-600 font-medium">
            Connect Supabase to see shelters
          </p>
          <p className="text-sm text-gray-500 mt-1">
            Over 40,000 shelters and rescue organizations will be listed here
          </p>
        </div>
      </section>
    </div>
  );
}
