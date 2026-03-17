import type { Metadata } from "next";
import Link from "next/link";

export async function generateMetadata({
  params,
}: {
  params: Promise<{ id: string }>;
}): Promise<Metadata> {
  const { id } = await params;
  return {
    title: `Shelter Profile - ${id}`,
    description:
      "View this shelter's profile, available dogs, and contact information. Help connect dogs with loving homes.",
    openGraph: {
      title: `Shelter Profile | WaitingTheLongest.com`,
      description:
        "View shelter details, available dogs, and how to adopt or volunteer.",
    },
  };
}

const PLACEHOLDER_SHELTER = {
  id: "shelter-1",
  name: "City Animal Shelter",
  shelter_type: "municipal" as const,
  address: "123 Main Street",
  city: "Austin",
  state_code: "TX",
  zip_code: "78701",
  phone: "(555) 123-4567",
  email: "info@cityshelter.example.com",
  website: "https://cityshelter.example.com",
  description:
    "City Animal Shelter is a municipal open-admission shelter serving the greater metro area. We are committed to finding loving homes for all animals in our care and work with rescue partners to ensure the best outcomes for every animal.",
  is_kill_shelter: true,
  accepts_rescue_pulls: true,
  total_dogs: 0,
  available_dogs: 0,
  critical_dogs: 0,
};

const SHELTER_TYPE_LABELS: Record<string, string> = {
  municipal: "Municipal Shelter",
  private: "Private Shelter",
  rescue: "Rescue Organization",
  foster_network: "Foster Network",
  humane_society: "Humane Society",
  spca: "SPCA",
};

export default async function ShelterProfilePage({
  params,
}: {
  params: Promise<{ id: string }>;
}) {
  const { id } = await params;
  const shelter = PLACEHOLDER_SHELTER;

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      {/* Breadcrumb */}
      <nav className="mb-6 text-sm text-gray-500" aria-label="Breadcrumb">
        <ol className="flex items-center gap-2">
          <li>
            <Link href="/" className="hover:text-gray-700">
              Home
            </Link>
          </li>
          <li>/</li>
          <li>
            <Link href="/shelters" className="hover:text-gray-700">
              Shelters
            </Link>
          </li>
          <li>/</li>
          <li className="text-gray-900 font-medium">{shelter.name}</li>
        </ol>
      </nav>

      {/* Placeholder Notice */}
      <div className="mb-6 p-4 bg-amber-50 border border-amber-200 rounded-lg text-sm text-amber-800">
        This is a placeholder profile. Connect Supabase to load real shelter data.
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
        {/* Main Content */}
        <div className="lg:col-span-2 space-y-6">
          {/* Shelter Header */}
          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <div className="flex flex-col sm:flex-row sm:items-start sm:justify-between gap-3 mb-4">
              <div>
                <h1 className="text-2xl md:text-3xl font-bold text-gray-900">
                  {shelter.name}
                </h1>
                <p className="text-gray-600 mt-1">
                  {shelter.city}, {shelter.state_code} {shelter.zip_code}
                </p>
              </div>
              <span className="inline-flex items-center px-3 py-1 text-sm bg-blue-50 text-blue-700 rounded-full font-medium">
                {SHELTER_TYPE_LABELS[shelter.shelter_type] || shelter.shelter_type}
              </span>
            </div>
            {shelter.description && (
              <p className="text-gray-700 leading-relaxed">
                {shelter.description}
              </p>
            )}
            <div className="flex flex-wrap gap-3 mt-4">
              {shelter.is_kill_shelter && (
                <span className="px-2.5 py-1 text-xs bg-red-50 text-red-700 rounded-full font-medium border border-red-200">
                  Open Admission (Kill Shelter)
                </span>
              )}
              {shelter.accepts_rescue_pulls && (
                <span className="px-2.5 py-1 text-xs bg-green-50 text-green-700 rounded-full font-medium border border-green-200">
                  Accepts Rescue Pulls
                </span>
              )}
            </div>
          </div>

          {/* Statistics */}
          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <h2 className="text-xl font-bold text-gray-900 mb-4">
              Statistics
            </h2>
            <div className="grid grid-cols-2 sm:grid-cols-4 gap-4">
              <StatCard label="Total Dogs" value="--" />
              <StatCard label="Available" value="--" />
              <StatCard label="Urgent" value="--" urgent />
              <StatCard label="Avg. Wait" value="-- days" />
            </div>
            <p className="text-xs text-gray-400 mt-4">
              Statistics will update in real-time once Supabase is connected
            </p>
          </div>

          {/* Dogs at This Shelter */}
          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <div className="flex items-center justify-between mb-4">
              <h2 className="text-xl font-bold text-gray-900">
                Dogs at {shelter.name}
              </h2>
              <Link
                href={`/dogs?shelter=${id}`}
                className="text-blue-600 hover:underline text-sm font-medium"
              >
                View All &rarr;
              </Link>
            </div>

            {/* Dog Card Placeholders */}
            <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
              {Array.from({ length: 4 }).map((_, i) => (
                <div
                  key={i}
                  className="flex gap-3 p-3 bg-gray-50 rounded-lg border border-gray-100"
                >
                  <div className="w-20 h-20 flex-shrink-0 bg-gray-200 rounded-lg flex items-center justify-center">
                    <svg
                      className="w-8 h-8 text-gray-400"
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
                  <div className="min-w-0">
                    <p className="font-medium text-gray-900 text-sm">
                      Dog Name
                    </p>
                    <p className="text-xs text-gray-500">Breed</p>
                    <p className="text-xs text-gray-400 mt-1">
                      Waiting -- days
                    </p>
                  </div>
                </div>
              ))}
            </div>

            <div className="text-center py-6 mt-4 bg-gray-50 rounded-lg border border-dashed border-gray-200">
              <p className="text-gray-500 text-sm">
                Connect Supabase to see dogs at this shelter
              </p>
            </div>
          </div>
        </div>

        {/* Sidebar */}
        <div className="space-y-6">
          {/* Contact Information */}
          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <h3 className="font-bold text-gray-900 mb-4">
              Contact Information
            </h3>
            <div className="space-y-4">
              <div>
                <p className="text-xs text-gray-500 uppercase tracking-wide mb-0.5">
                  Address
                </p>
                <p className="text-gray-900 text-sm">
                  {shelter.address}
                  <br />
                  {shelter.city}, {shelter.state_code} {shelter.zip_code}
                </p>
              </div>
              <div>
                <p className="text-xs text-gray-500 uppercase tracking-wide mb-0.5">
                  Phone
                </p>
                <a
                  href={`tel:${shelter.phone}`}
                  className="text-blue-600 hover:underline text-sm"
                >
                  {shelter.phone}
                </a>
              </div>
              <div>
                <p className="text-xs text-gray-500 uppercase tracking-wide mb-0.5">
                  Email
                </p>
                <a
                  href={`mailto:${shelter.email}`}
                  className="text-blue-600 hover:underline text-sm break-all"
                >
                  {shelter.email}
                </a>
              </div>
              <div>
                <p className="text-xs text-gray-500 uppercase tracking-wide mb-0.5">
                  Website
                </p>
                <a
                  href={shelter.website}
                  target="_blank"
                  rel="noopener noreferrer"
                  className="text-blue-600 hover:underline text-sm break-all"
                >
                  {shelter.website}
                </a>
              </div>
            </div>
          </div>

          {/* Location */}
          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <h3 className="font-bold text-gray-900 mb-3">Location</h3>
            <div className="aspect-[4/3] bg-gray-200 rounded-lg flex items-center justify-center">
              <div className="text-center text-gray-400">
                <svg
                  className="mx-auto w-10 h-10 mb-2"
                  fill="none"
                  viewBox="0 0 24 24"
                  stroke="currentColor"
                >
                  <path
                    strokeLinecap="round"
                    strokeLinejoin="round"
                    strokeWidth={1}
                    d="M17.657 16.657L13.414 20.9a1.998 1.998 0 01-2.827 0l-4.244-4.243a8 8 0 1111.314 0z"
                  />
                  <path
                    strokeLinecap="round"
                    strokeLinejoin="round"
                    strokeWidth={1}
                    d="M15 11a3 3 0 11-6 0 3 3 0 016 0z"
                  />
                </svg>
                <p className="text-xs">Map will display here</p>
              </div>
            </div>
            <Link
              href={`/states/${shelter.state_code.toLowerCase()}`}
              className="block mt-3 text-center text-sm text-blue-600 hover:underline"
            >
              View all shelters in {shelter.state_code}
            </Link>
          </div>

          {/* Actions */}
          <div className="bg-green-50 rounded-xl p-6 border border-green-200">
            <h3 className="font-bold text-green-900 mb-2">Want to Help?</h3>
            <p className="text-sm text-green-700 mb-4">
              This shelter needs fosters, volunteers, and adopters.
            </p>
            <div className="space-y-2">
              <Link
                href="/foster"
                className="block w-full text-center py-2.5 px-4 bg-green-600 hover:bg-green-700 text-white font-medium rounded-lg transition text-sm"
              >
                Become a Foster
              </Link>
              <button className="block w-full text-center py-2.5 px-4 bg-white hover:bg-green-50 text-green-700 font-medium rounded-lg transition text-sm border border-green-300">
                Share This Shelter
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

function StatCard({
  label,
  value,
  urgent = false,
}: {
  label: string;
  value: string;
  urgent?: boolean;
}) {
  return (
    <div className={`rounded-lg p-3 text-center ${urgent ? "bg-red-50" : "bg-gray-50"}`}>
      <p className="text-xs text-gray-500 mb-1">{label}</p>
      <p className={`text-xl font-bold ${urgent ? "text-red-600" : "text-gray-900"}`}>
        {value}
      </p>
    </div>
  );
}
