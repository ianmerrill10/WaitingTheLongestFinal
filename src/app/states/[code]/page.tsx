import type { Metadata } from "next";
import Link from "next/link";

const STATE_NAMES: Record<string, string> = {
  al: "Alabama", ak: "Alaska", az: "Arizona", ar: "Arkansas", ca: "California",
  co: "Colorado", ct: "Connecticut", de: "Delaware", fl: "Florida", ga: "Georgia",
  hi: "Hawaii", id: "Idaho", il: "Illinois", in: "Indiana", ia: "Iowa",
  ks: "Kansas", ky: "Kentucky", la: "Louisiana", me: "Maine", md: "Maryland",
  ma: "Massachusetts", mi: "Michigan", mn: "Minnesota", ms: "Mississippi", mo: "Missouri",
  mt: "Montana", ne: "Nebraska", nv: "Nevada", nh: "New Hampshire", nj: "New Jersey",
  nm: "New Mexico", ny: "New York", nc: "North Carolina", nd: "North Dakota", oh: "Ohio",
  ok: "Oklahoma", or: "Oregon", pa: "Pennsylvania", ri: "Rhode Island", sc: "South Carolina",
  sd: "South Dakota", tn: "Tennessee", tx: "Texas", ut: "Utah", vt: "Vermont",
  va: "Virginia", wa: "Washington", wv: "West Virginia", wi: "Wisconsin", wy: "Wyoming",
};

export async function generateMetadata({
  params,
}: {
  params: Promise<{ code: string }>;
}): Promise<Metadata> {
  const { code } = await params;
  const stateName = STATE_NAMES[code.toLowerCase()] || code.toUpperCase();
  return {
    title: `Adoptable Dogs in ${stateName}`,
    description: `Find adoptable shelter dogs and rescue organizations in ${stateName}. Browse dogs waiting for homes and local shelters near you.`,
    openGraph: {
      title: `Adoptable Dogs in ${stateName} | WaitingTheLongest.com`,
      description: `Browse shelter dogs and rescues in ${stateName}. Help a dog find a forever home.`,
    },
  };
}

export default async function StateDetailPage({
  params,
}: {
  params: Promise<{ code: string }>;
}) {
  const { code } = await params;
  const stateCode = code.toUpperCase();
  const stateName = STATE_NAMES[code.toLowerCase()] || stateCode;

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
            <Link href="/states" className="hover:text-gray-700">
              States
            </Link>
          </li>
          <li>/</li>
          <li className="text-gray-900 font-medium">{stateName}</li>
        </ol>
      </nav>

      {/* State Header */}
      <div className="mb-10">
        <div className="flex items-center gap-4 mb-3">
          <span className="text-4xl font-bold text-blue-600">{stateCode}</span>
          <h1 className="text-3xl md:text-4xl font-bold text-gray-900">
            {stateName}
          </h1>
        </div>
        <p className="text-gray-600 text-lg">
          Adoptable dogs and shelters in {stateName}.
        </p>
      </div>

      {/* Stats Overview */}
      <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-10">
        <StatCard label="Available Dogs" value="--" />
        <StatCard label="Urgent Dogs" value="--" urgent />
        <StatCard label="Shelters" value="--" />
        <StatCard label="Avg. Wait Time" value="-- days" />
      </div>

      {/* Dogs Section */}
      <section className="mb-12">
        <div className="flex items-center justify-between mb-6">
          <h2 className="text-2xl font-bold text-gray-900">
            Dogs in {stateName}
          </h2>
          <Link
            href={`/dogs?state=${stateCode}`}
            className="text-blue-600 hover:underline font-medium text-sm"
          >
            View All Dogs &rarr;
          </Link>
        </div>

        {/* Dog Card Placeholders */}
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-4">
          {Array.from({ length: 4 }).map((_, i) => (
            <div
              key={i}
              className="bg-white rounded-lg border border-gray-200 overflow-hidden shadow-sm"
            >
              <div className="aspect-square bg-gray-200 flex items-center justify-center">
                <svg
                  className="w-12 h-12 text-gray-400"
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
              <div className="p-3">
                <p className="font-medium text-gray-900 text-sm">Dog Name</p>
                <p className="text-xs text-gray-500">Breed &middot; Age</p>
              </div>
            </div>
          ))}
        </div>

        <div className="text-center py-8 mt-4 bg-gray-50 rounded-lg border border-dashed border-gray-300">
          <p className="text-gray-500">
            Connect Supabase to see dogs in {stateName}
          </p>
        </div>
      </section>

      {/* Shelters Section */}
      <section>
        <div className="flex items-center justify-between mb-6">
          <h2 className="text-2xl font-bold text-gray-900">
            Shelters in {stateName}
          </h2>
          <Link
            href={`/shelters?state=${stateCode}`}
            className="text-blue-600 hover:underline font-medium text-sm"
          >
            View All Shelters &rarr;
          </Link>
        </div>

        {/* Shelter Card Placeholders */}
        <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
          {Array.from({ length: 3 }).map((_, i) => (
            <div
              key={i}
              className="bg-white rounded-lg border border-gray-200 p-5 shadow-sm"
            >
              <h3 className="font-bold text-gray-900 mb-1">Shelter Name</h3>
              <p className="text-sm text-gray-600 mb-3">
                City, {stateCode}
              </p>
              <div className="flex items-center gap-4 text-xs text-gray-500">
                <span>-- dogs</span>
                <span>-- urgent</span>
              </div>
            </div>
          ))}
        </div>

        <div className="text-center py-8 mt-4 bg-gray-50 rounded-lg border border-dashed border-gray-300">
          <p className="text-gray-500">
            Connect Supabase to see shelters in {stateName}
          </p>
        </div>
      </section>
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
    <div
      className={`rounded-lg p-4 border ${
        urgent
          ? "bg-red-50 border-red-200"
          : "bg-white border-gray-200"
      }`}
    >
      <p className="text-xs text-gray-500 uppercase tracking-wide mb-1">
        {label}
      </p>
      <p
        className={`text-2xl font-bold ${
          urgent ? "text-red-600" : "text-gray-900"
        }`}
      >
        {value}
      </p>
    </div>
  );
}
