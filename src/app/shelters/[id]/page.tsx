import type { Metadata } from "next";
import Link from "next/link";
import { notFound } from "next/navigation";
import { createAdminClient } from "@/lib/supabase/admin";
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
    const supabase = createAdminClient();
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
    const supabase = createAdminClient();

    const { data: shelterData, error } = await supabase
      .from("shelters")
      .select("*")
      .eq("id", id)
      .single();

    if (error || !shelterData) {
      notFound();
    }
    shelter = shelterData;

    // Parallelize all queries — no !inner join needed since we filter by shelter_id
    const [dogsRes, availRes, urgentRes, totalRes] = await Promise.all([
      supabase
        .from("dogs")
        .select("*, shelters(name, city, state_code)")
        .eq("shelter_id", id)
        .eq("is_available", true)
        .order("intake_date", { ascending: true })
        .limit(24),
      supabase
        .from("dogs")
        .select("id", { count: "exact", head: true })
        .eq("shelter_id", id)
        .eq("is_available", true),
      supabase
        .from("dogs")
        .select("id", { count: "exact", head: true })
        .eq("shelter_id", id)
        .eq("is_available", true)
        .in("urgency_level", ["critical", "high"]),
      supabase
        .from("dogs")
        .select("id", { count: "exact", head: true })
        .eq("shelter_id", id),
    ]);

    dogs = dogsRes.data || [];
    availableCount = availRes.count;
    urgentCount = urgentRes.count;
    totalCount = totalRes.count;
  } catch (err) {
    console.error("[ShelterProfilePage] Failed to fetch data:", err);
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
            <div className="grid grid-cols-2 sm:grid-cols-3 gap-4">
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
            </div>
          </div>

          {/* Scraper & Data Status */}
          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <h2 className="text-xl font-bold text-gray-900 mb-4">
              Data Status
            </h2>
            <div className="grid grid-cols-2 gap-x-6 gap-y-3 text-sm">
              <div>
                <p className="text-xs text-gray-500 uppercase tracking-wide">Platform</p>
                <p className="text-gray-900 capitalize">{shelter.website_platform?.replace(/_/g, " ") || "Unknown"}</p>
              </div>
              <div>
                <p className="text-xs text-gray-500 uppercase tracking-wide">Scrape Status</p>
                <p className={`font-medium ${shelter.scrape_status === "success" ? "text-green-600" : shelter.scrape_status === "failed" ? "text-red-600" : "text-gray-600"}`}>
                  {shelter.scrape_status || "Pending"}
                </p>
              </div>
              <div>
                <p className="text-xs text-gray-500 uppercase tracking-wide">Last Scraped</p>
                <p className="text-gray-900">{shelter.last_scraped_at ? new Date(shelter.last_scraped_at).toLocaleString() : "Never"}</p>
              </div>
              <div>
                <p className="text-xs text-gray-500 uppercase tracking-wide">Scrape Count</p>
                <p className="text-gray-900">{shelter.scrape_count || 0}</p>
              </div>
              <div>
                <p className="text-xs text-gray-500 uppercase tracking-wide">Dogs Found Last Scrape</p>
                <p className="text-gray-900">{shelter.dogs_found_last_scrape ?? "N/A"}</p>
              </div>
              <div>
                <p className="text-xs text-gray-500 uppercase tracking-wide">Data Source</p>
                <p className="text-gray-900">{shelter.external_source || "Manual"}</p>
              </div>
              {shelter.ein && (
                <div>
                  <p className="text-xs text-gray-500 uppercase tracking-wide">EIN</p>
                  <p className="text-gray-900 font-mono">{shelter.ein}</p>
                </div>
              )}
              {shelter.partner_status && shelter.partner_status !== "unregistered" && (
                <div>
                  <p className="text-xs text-gray-500 uppercase tracking-wide">Partner Status</p>
                  <p className="text-gray-900 capitalize">{shelter.partner_status}</p>
                </div>
              )}
            </div>
            {shelter.adoptable_page_url && (
              <div className="mt-4 pt-3 border-t border-gray-100">
                <a
                  href={shelter.adoptable_page_url}
                  target="_blank"
                  rel="noopener noreferrer"
                  className="text-sm text-blue-600 hover:underline break-all"
                >
                  View Adoptable Animals Page
                </a>
              </div>
            )}
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

            {/* Quick Actions */}
            <div className="flex flex-wrap gap-2 mt-4 pt-4 border-t border-gray-100">
              {shelter.latitude && shelter.longitude && (
                <a
                  href={`https://www.google.com/maps/search/?api=1&query=${shelter.latitude},${shelter.longitude}`}
                  target="_blank"
                  rel="noopener noreferrer"
                  className="inline-flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium bg-blue-50 text-blue-700 rounded-full hover:bg-blue-100 transition"
                >
                  <svg className="w-3.5 h-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                    <path strokeLinecap="round" strokeLinejoin="round" d="M17.657 16.657L13.414 20.9a1.998 1.998 0 01-2.827 0l-4.244-4.243a8 8 0 1111.314 0z" />
                    <path strokeLinecap="round" strokeLinejoin="round" d="M15 11a3 3 0 11-6 0 3 3 0 016 0z" />
                  </svg>
                  Get Directions
                </a>
              )}
              {shelter.website && (
                <a
                  href={shelter.website}
                  target="_blank"
                  rel="noopener noreferrer"
                  className="inline-flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium bg-gray-50 text-gray-700 rounded-full hover:bg-gray-100 transition"
                >
                  <svg className="w-3.5 h-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                    <path strokeLinecap="round" strokeLinejoin="round" d="M10 6H6a2 2 0 00-2 2v10a2 2 0 002 2h10a2 2 0 002-2v-4M14 4h6m0 0v6m0-6L10 14" />
                  </svg>
                  Visit Website
                </a>
              )}
              {shelter.email && (
                <a
                  href={`mailto:${shelter.email}`}
                  className="inline-flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium bg-gray-50 text-gray-700 rounded-full hover:bg-gray-100 transition"
                >
                  <svg className="w-3.5 h-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                    <path strokeLinecap="round" strokeLinejoin="round" d="M3 8l7.89 5.26a2 2 0 002.22 0L21 8M5 19h14a2 2 0 002-2V7a2 2 0 00-2-2H5a2 2 0 00-2 2v10a2 2 0 002 2z" />
                  </svg>
                  Email
                </a>
              )}
              {shelter.phone && (
                <a
                  href={`tel:${shelter.phone}`}
                  className="inline-flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium bg-gray-50 text-gray-700 rounded-full hover:bg-gray-100 transition"
                >
                  <svg className="w-3.5 h-3.5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                    <path strokeLinecap="round" strokeLinejoin="round" d="M3 5a2 2 0 012-2h3.28a1 1 0 01.948.684l1.498 4.493a1 1 0 01-.502 1.21l-2.257 1.13a11.042 11.042 0 005.516 5.516l1.13-2.257a1 1 0 011.21-.502l4.493 1.498a1 1 0 01.684.949V19a2 2 0 01-2 2h-1C9.716 21 3 14.284 3 6V5z" />
                  </svg>
                  Call
                </a>
              )}
            </div>
          </div>

          {/* Location */}
          {shelter.latitude && shelter.longitude ? (
            <div className="bg-white rounded-xl shadow-sm border border-gray-200 overflow-hidden">
              <h3 className="font-bold text-gray-900 p-6 pb-3">Location</h3>
              <iframe
                title={`Map of ${shelter.name}`}
                width="100%"
                height="300"
                style={{ border: 0 }}
                loading="lazy"
                src={`https://www.openstreetmap.org/export/embed.html?bbox=${shelter.longitude - 0.02}%2C${shelter.latitude - 0.015}%2C${shelter.longitude + 0.02}%2C${shelter.latitude + 0.015}&layer=mapnik&marker=${shelter.latitude}%2C${shelter.longitude}`}
              />
              <div className="p-3 text-center space-y-2">
                <a
                  href={`https://www.google.com/maps/search/?api=1&query=${shelter.latitude},${shelter.longitude}`}
                  target="_blank"
                  rel="noopener noreferrer"
                  className="text-sm text-blue-600 hover:underline"
                >
                  Get Directions
                </a>
                <Link
                  href={`/states/${shelter.state_code.toLowerCase()}`}
                  className="block text-sm text-blue-600 hover:underline"
                >
                  View all shelters in {shelter.state_code}
                </Link>
              </div>
            </div>
          ) : shelter.city && shelter.state_code ? (
            <div className="bg-white rounded-xl shadow-sm border border-gray-200 overflow-hidden">
              <h3 className="font-bold text-gray-900 p-6 pb-3">Location</h3>
              <iframe
                title={`Map of ${shelter.city}, ${shelter.state_code}`}
                width="100%"
                height="300"
                style={{ border: 0 }}
                loading="lazy"
                src={`https://www.openstreetmap.org/export/embed.html?bbox=-180%2C-90%2C180%2C90&layer=mapnik`}
              />
              <div className="p-3 text-center space-y-2">
                <p className="text-sm text-gray-500">
                  {shelter.city}, {shelter.state_code}
                </p>
                <Link
                  href={`/states/${shelter.state_code.toLowerCase()}`}
                  className="block text-sm text-blue-600 hover:underline"
                >
                  View all shelters in {shelter.state_code}
                </Link>
              </div>
            </div>
          ) : null}

          {/* Social Media & Links */}
          {(shelter.facebook_url || shelter.social_instagram || shelter.social_tiktok) && (
            <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
              <h3 className="font-bold text-gray-900 mb-3">Social Media</h3>
              <div className="space-y-2">
                {shelter.facebook_url && (
                  <a href={shelter.facebook_url} target="_blank" rel="noopener noreferrer" className="flex items-center gap-2 text-sm text-blue-600 hover:underline">
                    <span className="w-5 text-center">f</span> Facebook
                  </a>
                )}
                {shelter.social_instagram && (
                  <a href={`https://instagram.com/${shelter.social_instagram}`} target="_blank" rel="noopener noreferrer" className="flex items-center gap-2 text-sm text-pink-600 hover:underline">
                    <span className="w-5 text-center">@</span> {shelter.social_instagram}
                  </a>
                )}
                {shelter.social_tiktok && (
                  <a href={`https://tiktok.com/@${shelter.social_tiktok}`} target="_blank" rel="noopener noreferrer" className="flex items-center gap-2 text-sm text-gray-900 hover:underline">
                    <span className="w-5 text-center">T</span> {shelter.social_tiktok}
                  </a>
                )}
              </div>
            </div>
          )}

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
              <a
                href={`https://twitter.com/intent/tweet?text=${encodeURIComponent(`Help dogs at ${shelter.name} find homes!`)}&url=${encodeURIComponent(`https://waitingthelongest.com/shelters/${id}`)}`}
                target="_blank"
                rel="noopener noreferrer"
                className="block w-full text-center py-2.5 px-4 bg-white hover:bg-green-50 text-green-700 font-medium rounded-lg transition text-sm border border-green-300"
              >
                Share This Shelter
              </a>
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
