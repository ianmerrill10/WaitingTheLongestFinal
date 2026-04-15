import type { Metadata } from "next";
import Link from "next/link";
import { createAdminClient } from "@/lib/supabase/admin";
import DogGrid from "@/components/dogs/DogGrid";
import NationalAvgCounter from "@/components/counters/NationalAvgCounter";

export const dynamic = "force-dynamic";

export const metadata: Metadata = {
  title: "WaitingTheLongest.com — Every Shelter Dog in America, Ranked by Wait Time",
  description: "Real-time countdown timers for shelter dogs facing euthanasia. See which dogs have been waiting the longest and help save a life before time runs out.",
  openGraph: {
    title: "WaitingTheLongest.com — Stop a Clock, Save a Life",
    description: "Real-time countdown timers for shelter dogs facing euthanasia. See which dogs have been waiting the longest and help save a life before time runs out.",
    url: "https://waitingthelongest.com",
    siteName: "WaitingTheLongest.com",
    type: "website",
  },
  twitter: {
    card: "summary_large_image",
    title: "WaitingTheLongest.com — Stop a Clock, Save a Life",
    description: "Real-time countdown timers for shelter dogs facing euthanasia.",
  },
};

export default async function HomePage() {
  let totalDogs = 0;
  let urgentCount = 0;
  let shelterCount = 0;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  let urgentDogs: any[] = [];
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  let longestWaiting: any[] = [];
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  let overlookedDogs: any[] = [];

  try {
    const supabase = createAdminClient();

    // Fetch stats — use allSettled so one failure doesn't break the page
    const [dogsResult, urgentResult, shelterResult] = await Promise.allSettled([
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true),
      supabase.from("dogs").select("id", { count: "exact", head: true }).eq("is_available", true).in("urgency_level", ["critical", "high"]).gt("euthanasia_date", new Date().toISOString()),
      supabase.from("shelters").select("id", { count: "exact", head: true }),
    ]);

    totalDogs = dogsResult.status === "fulfilled" ? (dogsResult.value.count || 0) : 0;
    urgentCount = urgentResult.status === "fulfilled" ? (urgentResult.value.count || 0) : 0;
    shelterCount = shelterResult.status === "fulfilled" ? (shelterResult.value.count || 0) : 0;

    // Fairness rotation: fetch larger pools, then rotate 6-dog windows every hour.
    // This ensures dogs beyond the absolute top still get homepage exposure.
    // Critical/urgent dogs (euthanasia imminent) always show the most urgent first.
    const hourSeed = new Date().getUTCHours();

    // Fetch urgent dogs — always show the top 3 most urgent, rotate positions 4-6
    // from a pool of 15. This guarantees the closest-to-euthanasia dog is ALWAYS seen.
    const urgentRes = await supabase
      .from("dogs")
      .select("*, shelters!dogs_shelter_id_fkey!inner(name, city, state_code)")
      .eq("is_available", true)
      .in("urgency_level", ["critical", "high"])
      .gt("euthanasia_date", new Date().toISOString())
      .order("euthanasia_date", { ascending: true, nullsFirst: false })
      .limit(15);
    const urgentPool = urgentRes.data || [];
    const urgentTop = urgentPool.slice(0, 3);
    const urgentRest = urgentPool.slice(3);
    const urgentOffset = urgentRest.length > 0 ? (hourSeed * 3) % urgentRest.length : 0;
    const urgentRotated: typeof urgentRest = [];
    for (let i = 0; i < 3 && i < urgentRest.length; i++) {
      urgentRotated.push(urgentRest[(urgentOffset + i) % urgentRest.length]);
    }
    urgentDogs = [...urgentTop, ...urgentRotated];

    // Longest waiting — fetch top 30, rotate a 6-dog window by hour
    // Use ranking_eligible flag — only dogs with verified intake dates rank here
    let waitingRes = await supabase
      .from("dogs")
      .select("*, shelters!dogs_shelter_id_fkey!inner(name, city, state_code)")
      .eq("is_available", true)
      .eq("ranking_eligible", true)
      .order("intake_date", { ascending: true })
      .limit(30);

    // Fallback: if ranking_eligible hasn't been backfilled yet, use confidence filter
    if (!waitingRes.data || waitingRes.data.length === 0) {
      waitingRes = await supabase
        .from("dogs")
        .select("*, shelters!dogs_shelter_id_fkey!inner(name, city, state_code)")
        .eq("is_available", true)
        .in("date_confidence", ["verified", "high", "medium"])
        .order("intake_date", { ascending: true })
        .limit(30);
    }
    const waitingPool = waitingRes.data || [];
    // Always show the #1 longest waiting dog, rotate positions 2-6 from the rest
    const waitingTop = waitingPool.slice(0, 1);
    const waitingRest = waitingPool.slice(1);
    const waitingOffset = waitingRest.length > 0 ? (hourSeed * 5) % waitingRest.length : 0;
    const waitingRotated: typeof waitingRest = [];
    for (let i = 0; i < 5 && i < waitingRest.length; i++) {
      waitingRotated.push(waitingRest[(waitingOffset + i) % waitingRest.length]);
    }
    longestWaiting = [...waitingTop, ...waitingRotated];

    // Overlooked dogs — fetch top 24, rotate 6-dog window by hour
    const overlookedRes = await supabase
      .from("dogs")
      .select("*, shelters!dogs_shelter_id_fkey!inner(name, city, state_code)")
      .eq("is_available", true)
      .or("age_category.eq.senior,has_special_needs.eq.true")
      .order("intake_date", { ascending: true })
      .limit(24);
    const overlookedPool = overlookedRes.data || [];
    const overlookedOffset = overlookedPool.length > 0 ? (hourSeed * 6) % overlookedPool.length : 0;
    const overlookedRotated: typeof overlookedPool = [];
    for (let i = 0; i < 6 && i < overlookedPool.length; i++) {
      overlookedRotated.push(overlookedPool[(overlookedOffset + i) % overlookedPool.length]);
    }
    overlookedDogs = overlookedRotated;
  } catch (err) {
    console.error("[HomePage] Failed to fetch data:", err);
  }

  return (
    <div>
      {/* Hero Section */}
      <section className="bg-wtl-dark text-white">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-16 md:py-24">
          <div className="text-center">
            <h1 className="text-4xl md:text-6xl font-extrabold tracking-tight mb-4">
              Stop a Clock, <span className="text-led-green">Save a Life</span>
            </h1>
            <p className="text-xl md:text-2xl text-gray-300 max-w-3xl mx-auto mb-8">
              Find adoptable dogs from shelters and rescues across America.
              Real-time countdown timers for dogs on euthanasia lists.
              Help save a life today.
            </p>

            {/* Search Bar */}
            <div className="max-w-2xl mx-auto mb-12">
              <form action="/dogs" method="GET" className="flex gap-2">
                <input
                  type="text"
                  name="q"
                  placeholder="Search by breed, name, or location..."
                  className="flex-1 px-6 py-4 rounded-lg bg-white/10 border border-white/20 text-white placeholder-gray-400 focus:outline-none focus:ring-2 focus:ring-led-green focus:border-transparent text-lg"
                />
                <button
                  type="submit"
                  className="px-8 py-4 bg-led-green text-black font-bold rounded-lg hover:bg-green-400 transition"
                >
                  Search
                </button>
              </form>
              <div className="flex justify-center gap-4 mt-4">
                <Link href="/dogs?sort=random" className="text-sm text-gray-400 hover:text-led-green transition">
                  Show me a random dog
                </Link>
              </div>
            </div>

            {/* National Average Wait Time */}
            <div className="max-w-xl mx-auto mb-8">
              <NationalAvgCounter />
            </div>

            {/* Stats */}
            <div className="grid grid-cols-2 md:grid-cols-4 gap-4 max-w-4xl mx-auto">
              <StatCard value={shelterCount > 0 ? shelterCount.toLocaleString() : "--"} label="Organizations Tracked" />
              <StatCard value="50" label="States Covered" />
              <StatCard value={totalDogs > 0 ? totalDogs.toLocaleString() : "--"} label="Dogs Available" />
              <StatCard value={urgentCount > 0 ? urgentCount.toLocaleString() : "--"} label="Urgent Dogs" urgent />
            </div>
          </div>
        </div>
      </section>

      {/* Urgent Dogs Section */}
      <section className="py-12 bg-red-50 border-y-2 border-red-200">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex items-center justify-between mb-8">
            <div className="flex items-center gap-3">
              <span className="inline-block w-3 h-3 bg-red-500 rounded-full" />
              <h2 className="text-2xl md:text-3xl font-bold text-red-800">
                Urgent: These Dogs Need You NOW
              </h2>
            </div>
            <Link href="/urgent" className="text-red-600 hover:text-red-800 font-semibold">
              View All &rarr;
            </Link>
          </div>
          {urgentDogs && urgentDogs.length > 0 ? (
            <DogGrid dogs={urgentDogs} showCountdown urgencyHighlight />
          ) : (
            <div className="text-center py-12 text-gray-500">
              <p className="text-lg">No dogs currently on urgent euthanasia lists.</p>
              <p className="text-sm mt-2">When dogs are flagged as urgent, they&apos;ll appear here with real-time countdown timers.</p>
            </div>
          )}
        </div>
      </section>

      {/* Longest Waiting Section */}
      <section className="py-12">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex items-center justify-between mb-8">
            <div>
              <h2 className="text-2xl md:text-3xl font-bold text-gray-900">Waiting the Longest</h2>
              <p className="mt-2 max-w-2xl text-sm text-gray-600">
                Ranked from the earliest verified intake dates first. Dogs without a trustworthy intake date stay out of this section until we can verify them.
              </p>
            </div>
            <Link href="/dogs?sort=wait_time" className="text-wtl-blue hover:underline font-semibold">
              View All &rarr;
            </Link>
          </div>
          {longestWaiting && longestWaiting.length > 0 ? (
            <DogGrid dogs={longestWaiting} />
          ) : (
            <div className="text-center py-12 text-gray-500">
              <p className="text-lg">Dogs sorted by how long they&apos;ve been waiting will appear here with LED wait time counters.</p>
            </div>
          )}
        </div>
      </section>

      {/* Overlooked Angels Section */}
      <section className="py-12 bg-purple-50">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex items-center justify-between mb-8">
            <h2 className="text-2xl md:text-3xl font-bold text-purple-900">Overlooked Angels</h2>
            <Link href="/overlooked" className="text-purple-600 hover:underline font-semibold">
              Learn More &rarr;
            </Link>
          </div>
          <p className="text-gray-600 mb-8 max-w-3xl">
            These dogs are statistically most likely to be euthanized. Senior dogs,
            black dogs, pit bulls, bonded pairs, and special needs dogs deserve love too.
          </p>
          <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-6 gap-4 mb-8">
            {[
              { label: "Senior Dogs", desc: "7+ years of love to give" },
              { label: "Black Dogs", desc: "Overlooked but beautiful" },
              { label: "Pit Bulls", desc: "Misunderstood & loyal" },
              { label: "Bonded Pairs", desc: "Better together" },
              { label: "Special Needs", desc: "Extra special love" },
              { label: "Long-Timers", desc: "6+ months waiting" },
            ].map((cat) => (
              <Link
                key={cat.label}
                href={`/overlooked#${cat.label.toLowerCase().replace(/ /g, "-")}`}
                className="bg-white rounded-lg p-4 text-center shadow-sm hover:shadow-md transition"
              >
                <div className="font-semibold text-sm">{cat.label}</div>
                <div className="text-xs text-gray-500 mt-1">{cat.desc}</div>
              </Link>
            ))}
          </div>
          {overlookedDogs && overlookedDogs.length > 0 && (
            <DogGrid dogs={overlookedDogs} />
          )}
        </div>
      </section>

      {/* Browse by State */}
      <section className="py-12">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <h2 className="text-2xl md:text-3xl font-bold text-gray-900 mb-8">Browse by State</h2>
          <div className="grid grid-cols-5 md:grid-cols-10 gap-2">
            {US_STATES.map((code) => (
              <Link
                key={code}
                href={`/states/${code.toLowerCase()}`}
                className="text-center py-2 px-1 rounded bg-white border border-gray-200 hover:border-wtl-blue hover:bg-blue-50 transition text-sm font-medium"
              >
                {code}
              </Link>
            ))}
          </div>
        </div>
      </section>

      {/* CTA: Foster */}
      <section className="py-16 bg-green-600 text-white">
        <div className="max-w-4xl mx-auto px-4 text-center">
          <h2 className="text-3xl md:text-4xl font-bold mb-4">Become a Foster Hero</h2>
          <p className="text-xl text-green-100 mb-8">
            Foster homes have 14x better adoption outcomes than shelters. Open your home, save a life.
          </p>
          <Link
            href="/foster"
            className="inline-block px-8 py-4 bg-white text-green-700 font-bold rounded-lg hover:bg-green-50 transition text-lg"
          >
            Sign Up to Foster
          </Link>
        </div>
      </section>
    </div>
  );
}

function StatCard({ value, label, urgent = false }: { value: string; label: string; urgent?: boolean }) {
  return (
    <div className={`rounded-lg p-4 ${urgent ? "bg-red-600/20 border border-red-500/30" : "bg-white/10 border border-white/10"}`}>
      <div className={`text-3xl font-bold ${urgent ? "text-red-400" : "text-led-green"}`}>{value}</div>
      <div className="text-sm text-gray-300 mt-1">{label}</div>
    </div>
  );
}

const US_STATES = [
  "AL","AK","AZ","AR","CA","CO","CT","DE","FL","GA",
  "HI","ID","IL","IN","IA","KS","KY","LA","ME","MD",
  "MA","MI","MN","MS","MO","MT","NE","NV","NH","NJ",
  "NM","NY","NC","ND","OH","OK","OR","PA","RI","SC",
  "SD","TN","TX","UT","VT","VA","WA","WV","WI","WY",
];
