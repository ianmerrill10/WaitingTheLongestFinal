import type { Metadata } from "next";
import Link from "next/link";
import { createAdminClient } from "@/lib/supabase/admin";
import DogGrid from "@/components/dogs/DogGrid";

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

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  let dogs: any[] = [];
  let urgentCount: number | null = 0;
  let totalDogsCount: number | null = 0;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  let shelters: any[] = [];
  let shelterCount = 0;

  try {
    const supabase = createAdminClient();

    // First get shelter IDs for this state — avoids unreliable !inner join counts
    const { data: stateShelters } = await supabase
      .from("shelters")
      .select("id")
      .eq("state_code", stateCode);

    const shelterIds = (stateShelters || []).map((s) => s.id);
    shelterCount = shelterIds.length;

    if (shelterIds.length > 0) {
      const [dogsRes, urgentRes, totalRes, shelterRes] = await Promise.all([
        supabase
          .from("dogs")
          .select("*, shelters(name, city, state_code)")
          .eq("is_available", true)
          .in("shelter_id", shelterIds)
          .order("intake_date", { ascending: true })
          .limit(24),
        supabase
          .from("dogs")
          .select("id", { count: "exact", head: true })
          .eq("is_available", true)
          .in("shelter_id", shelterIds)
          .in("urgency_level", ["critical", "high"]),
        supabase
          .from("dogs")
          .select("id", { count: "exact", head: true })
          .eq("is_available", true)
          .in("shelter_id", shelterIds),
        supabase
          .from("shelters")
          .select("*")
          .eq("state_code", stateCode)
          .order("name", { ascending: true })
          .limit(30),
      ]);

      dogs = dogsRes.data || [];
      urgentCount = urgentRes.count;
      totalDogsCount = totalRes.count;
      shelters = shelterRes.data || [];
    } else {
      // No shelters in this state — get shelter list anyway for display
      const { data: shelterRes } = await supabase
        .from("shelters")
        .select("*")
        .eq("state_code", stateCode)
        .order("name", { ascending: true })
        .limit(30);
      shelters = shelterRes || [];
    }
  } catch {
    // Supabase not configured yet
  }

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
      <div className="grid grid-cols-2 md:grid-cols-3 gap-4 mb-10">
        <StatCard
          label="Available Dogs"
          value={String(totalDogsCount ?? 0)}
        />
        <StatCard
          label="Urgent Dogs"
          value={String(urgentCount ?? 0)}
          urgent
        />
        <StatCard label="Shelters" value={String(shelterCount)} />
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

        <DogGrid
          dogs={dogs ?? []}
          showCountdown
          urgencyHighlight
          emptyMessage={`No dogs currently available in ${stateName}.`}
        />
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

        {shelters && shelters.length > 0 ? (
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
            {shelters.map((shelter) => (
              <Link
                key={shelter.id}
                href={`/shelters/${shelter.id}`}
                className="bg-white rounded-lg border border-gray-200 p-5 shadow-sm hover:shadow-md hover:border-blue-300 transition block"
              >
                <h3 className="font-bold text-gray-900 mb-1">
                  {shelter.name}
                </h3>
                <p className="text-sm text-gray-600 mb-3">
                  {shelter.city}, {shelter.state_code}
                </p>
                {shelter.shelter_type && (
                  <span className="px-2 py-0.5 text-xs bg-gray-100 text-gray-600 rounded-full capitalize">
                    {shelter.shelter_type.replace(/_/g, " ")}
                  </span>
                )}
                {shelter.is_kill_shelter && (
                  <span className="ml-2 px-2 py-0.5 text-xs bg-red-50 text-red-600 rounded-full">
                    Kill Shelter
                  </span>
                )}
              </Link>
            ))}
          </div>
        ) : (
          <div className="text-center py-8 bg-gray-50 rounded-lg border border-dashed border-gray-300">
            <p className="text-gray-500">
              No shelters currently listed in {stateName}.
            </p>
          </div>
        )}
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
