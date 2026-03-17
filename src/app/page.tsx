import Link from "next/link";

// Demo data for initial build - will be replaced with Supabase queries
const DEMO_STATS = {
  totalDogs: 0,
  urgentDogs: 0,
  shelters: 40085,
  statesCovered: 50,
};

export default function HomePage() {
  return (
    <div>
      {/* Hero Section */}
      <section className="bg-wtl-dark text-white">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-16 md:py-24">
          <div className="text-center">
            <h1 className="text-4xl md:text-6xl font-extrabold tracking-tight mb-4">
              Every Day{" "}
              <span className="text-led-green">Matters</span>
            </h1>
            <p className="text-xl md:text-2xl text-gray-300 max-w-3xl mx-auto mb-8">
              Find adoptable dogs from shelters and rescues across America.
              Real-time countdown timers for dogs on euthanasia lists.
              Help save a life today.
            </p>

            {/* Search Bar */}
            <div className="max-w-2xl mx-auto mb-12">
              <form action="/search" method="GET" className="flex gap-2">
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
            </div>

            {/* Stats */}
            <div className="grid grid-cols-2 md:grid-cols-4 gap-4 max-w-4xl mx-auto">
              <StatCard
                value={DEMO_STATS.shelters.toLocaleString()}
                label="Organizations Tracked"
              />
              <StatCard
                value={DEMO_STATS.statesCovered.toString()}
                label="States Covered"
              />
              <StatCard
                value="--"
                label="Dogs Available"
              />
              <StatCard
                value="--"
                label="Urgent Dogs"
                urgent
              />
            </div>
          </div>
        </div>
      </section>

      {/* Urgent Dogs Section */}
      <section className="py-12 bg-red-50 border-y-2 border-red-200">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex items-center justify-between mb-8">
            <div className="flex items-center gap-3">
              <span className="inline-block w-3 h-3 bg-red-500 rounded-full animate-pulse" />
              <h2 className="text-2xl md:text-3xl font-bold text-red-800">
                Urgent: These Dogs Need You NOW
              </h2>
            </div>
            <Link
              href="/urgent"
              className="text-red-600 hover:text-red-800 font-semibold"
            >
              View All &rarr;
            </Link>
          </div>
          <div className="text-center py-12 text-gray-500">
            <p className="text-lg">
              Connect your Supabase database and RescueGroups API key to see urgent dogs here.
            </p>
            <p className="text-sm mt-2">
              Dogs on euthanasia lists will appear with real-time countdown timers.
            </p>
          </div>
        </div>
      </section>

      {/* Longest Waiting Section */}
      <section className="py-12">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex items-center justify-between mb-8">
            <h2 className="text-2xl md:text-3xl font-bold text-gray-900">
              Waiting the Longest
            </h2>
            <Link
              href="/dogs?sort=wait_time"
              className="text-wtl-blue hover:underline font-semibold"
            >
              View All &rarr;
            </Link>
          </div>
          <div className="text-center py-12 text-gray-500">
            <p className="text-lg">
              Dogs sorted by how long they&apos;ve been waiting will appear here with LED wait time counters.
            </p>
          </div>
        </div>
      </section>

      {/* Overlooked Angels Section */}
      <section className="py-12 bg-purple-50">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex items-center justify-between mb-8">
            <h2 className="text-2xl md:text-3xl font-bold text-purple-900">
              Overlooked Angels
            </h2>
            <Link
              href="/overlooked"
              className="text-purple-600 hover:underline font-semibold"
            >
              Learn More &rarr;
            </Link>
          </div>
          <p className="text-gray-600 mb-8 max-w-3xl">
            These dogs are statistically most likely to be euthanized. Senior dogs,
            black dogs, pit bulls, bonded pairs, and special needs dogs deserve love too.
          </p>
          <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-6 gap-4">
            {[
              { label: "Senior Dogs", desc: "7+ years of love to give", icon: "🤍" },
              { label: "Black Dogs", desc: "Overlooked but beautiful", icon: "🖤" },
              { label: "Pit Bulls", desc: "Misunderstood & loyal", icon: "💪" },
              { label: "Bonded Pairs", desc: "Better together", icon: "💕" },
              { label: "Special Needs", desc: "Extra special love", icon: "⭐" },
              { label: "Long-Timers", desc: "6+ months waiting", icon: "⏰" },
            ].map((cat) => (
              <Link
                key={cat.label}
                href={`/overlooked#${cat.label.toLowerCase().replace(/ /g, "-")}`}
                className="bg-white rounded-lg p-4 text-center shadow-sm hover:shadow-md transition"
              >
                <div className="text-3xl mb-2">{cat.icon}</div>
                <div className="font-semibold text-sm">{cat.label}</div>
                <div className="text-xs text-gray-500 mt-1">{cat.desc}</div>
              </Link>
            ))}
          </div>
        </div>
      </section>

      {/* Browse by State */}
      <section className="py-12">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <h2 className="text-2xl md:text-3xl font-bold text-gray-900 mb-8">
            Browse by State
          </h2>
          <div className="grid grid-cols-5 md:grid-cols-10 gap-2">
            {US_STATES.map((state) => (
              <Link
                key={state.code}
                href={`/states/${state.code.toLowerCase()}`}
                className="text-center py-2 px-1 rounded bg-white border border-gray-200 hover:border-wtl-blue hover:bg-blue-50 transition text-sm font-medium"
              >
                {state.code}
              </Link>
            ))}
          </div>
        </div>
      </section>

      {/* CTA: Foster */}
      <section className="py-16 bg-green-600 text-white">
        <div className="max-w-4xl mx-auto px-4 text-center">
          <h2 className="text-3xl md:text-4xl font-bold mb-4">
            Become a Foster Hero
          </h2>
          <p className="text-xl text-green-100 mb-8">
            Foster homes have 14x better adoption outcomes than shelters.
            Open your home, save a life.
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

function StatCard({
  value,
  label,
  urgent = false,
}: {
  value: string;
  label: string;
  urgent?: boolean;
}) {
  return (
    <div
      className={`rounded-lg p-4 ${
        urgent
          ? "bg-red-600/20 border border-red-500/30"
          : "bg-white/10 border border-white/10"
      }`}
    >
      <div
        className={`text-3xl font-bold ${urgent ? "text-red-400" : "text-led-green"}`}
      >
        {value}
      </div>
      <div className="text-sm text-gray-300 mt-1">{label}</div>
    </div>
  );
}

const US_STATES = [
  { code: "AL" }, { code: "AK" }, { code: "AZ" }, { code: "AR" }, { code: "CA" },
  { code: "CO" }, { code: "CT" }, { code: "DE" }, { code: "FL" }, { code: "GA" },
  { code: "HI" }, { code: "ID" }, { code: "IL" }, { code: "IN" }, { code: "IA" },
  { code: "KS" }, { code: "KY" }, { code: "LA" }, { code: "ME" }, { code: "MD" },
  { code: "MA" }, { code: "MI" }, { code: "MN" }, { code: "MS" }, { code: "MO" },
  { code: "MT" }, { code: "NE" }, { code: "NV" }, { code: "NH" }, { code: "NJ" },
  { code: "NM" }, { code: "NY" }, { code: "NC" }, { code: "ND" }, { code: "OH" },
  { code: "OK" }, { code: "OR" }, { code: "PA" }, { code: "RI" }, { code: "SC" },
  { code: "SD" }, { code: "TN" }, { code: "TX" }, { code: "UT" }, { code: "VT" },
  { code: "VA" }, { code: "WA" }, { code: "WV" }, { code: "WI" }, { code: "WY" },
];
