import type { Metadata } from "next";
import Link from "next/link";
import { notFound } from "next/navigation";
import { createClient } from "@/lib/supabase/server";
import DogGrid from "@/components/dogs/DogGrid";

const SHELTER_TYPE_LABELS: Record<string, string> = {
  municipal: "Municipal Shelter",
  private: "Private Shelter",
  rescue: "Rescue Organization",
  foster_network: "Foster Network",
  humane_society: "Humane Society",
  spca: "SPCA",
};

export async function generateMetadata({
  params,
}: {
  params: Promise<{ id: string }>;
}): Promise<Metadata> {
  try {
    const { id } = await params;
    const supabase = await createClient();
    const { data: shelter } = await supabase
      .from("shelters")
      .select("name, city, state_code")
      .eq("id", id)
      .single();

    const shelterName = shelter?.name ?? "Shelter Profile";
    const location = shelter
      ? `${shelter.city}, ${shelter.state_code}`
      : "";

    return {
      title: `${shelterName} - Shelter Profile`,
      description: `View ${shelterName}${location ? ` in ${location}` : ""} - available dogs, contact information, and how to adopt or volunteer.`,
      openGraph: {
        title: `${shelterName} | WaitingTheLongest.com`,
        description: `View shelter details, available dogs, and how to adopt or volunteer.`,
      },
    };
  } catch {
    return { title: "Shelter Profile" };
  }
}

export default async function ShelterProfilePage({
  params,
}: {
  params: Promise<{ id: string }>;
}) {
  const { id } = await params;

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  let shelter: any = null;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  let dogs: any[] = [];
  let availableCount: number | null = 0;
  let urgentCount: number | null = 0;
  let totalCount: number | null = 0;

  try {
    const supabase = await createClient();

    const { data: shelterData, error } = await supabase
      .from("shelters")
      .select("*")
      .eq("id", id)
      .single();

    if (error || !shelterData) {
      notFound();
    }
    shelter = shelterData;

    const dogsRes = await supabase
      .from("dogs")
      .select("*, shelters!inner(name, city, state_code)")
      .eq("shelter_id", id)
      .eq("is_available", true)
      .order("intake_date", { ascending: true })
      .limit(12);
    dogs = dogsRes.data || [];

    const availRes = await supabase
      .from("dogs")
      .select("id", { count: "exact", head: true })
      .eq("shelter_id", id)
      .eq("is_available", true);
    availableCount = availRes.count;

    const urgentRes = await supabase
      .from("dogs")
      .select("id", { count: "exact", head: true })
      .eq("shelter_id", id)
      .eq("is_available", true)
      .in("urgency_level", ["critical", "urgent"]);
    urgentCount = urgentRes.count;

    const totalRes = await supabase
      .from("dogs")
      .select("id", { count: "exact", head: true })
      .eq("shelter_id", id);
    totalCount = totalRes.count;
  } catch {
    notFound();
  }

  if (!shelter) {
    notFound();
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
            <Link href="/shelters" className="hover:text-gray-700">
              Shelters
            </Link>
          </li>
          <li>/</li>
          <li className="text-gray-900 font-medium">{shelter.name}</li>
        </ol>
      </nav>

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
                  {shelter.city}, {shelter.state_code}{" "}
                  {shelter.zip_code ?? ""}
                </p>
              </div>
              {shelter.shelter_type && (
                <span className="inline-flex items-center px-3 py-1 text-sm bg-blue-50 text-blue-700 rounded-full font-medium">
                  {SHELTER_TYPE_LABELS[shelter.shelter_type] ||
                    shelter.shelter_type.replace(/_/g, " ")}
                </span>
              )}
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
              <StatCard label="Total Dogs" value={String(totalCount ?? 0)} />
              <StatCard
                label="Available"
                value={String(availableCount ?? 0)}
              />
              <StatCard
                label="Urgent"
                value={String(urgentCount ?? 0)}
                urgent
              />
              <StatCard
                label="Showing"
                value={`${dogs?.length ?? 0} dogs`}
              />
            </div>
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

            <DogGrid
              dogs={dogs ?? []}
              showCountdown
              urgencyHighlight
              emptyMessage="No dogs currently available at this shelter."
            />
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
              {(shelter.address || shelter.city) && (
                <div>
                  <p className="text-xs text-gray-500 uppercase tracking-wide mb-0.5">
                    Address
                  </p>
                  <p className="text-gray-900 text-sm">
                    {shelter.address && (
                      <>
                        {shelter.address}
                        <br />
                      </>
                    )}
                    {shelter.city}, {shelter.state_code}{" "}
                    {shelter.zip_code ?? ""}
                  </p>
                </div>
              )}
              {shelter.phone && (
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
              )}
              {shelter.email && (
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
              )}
              {shelter.website && (
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
              )}
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
    <div
      className={`rounded-lg p-3 text-center ${
        urgent ? "bg-red-50" : "bg-gray-50"
      }`}
    >
      <p className="text-xs text-gray-500 mb-1">{label}</p>
      <p
        className={`text-xl font-bold ${
          urgent ? "text-red-600" : "text-gray-900"
        }`}
      >
        {value}
      </p>
    </div>
  );
}
