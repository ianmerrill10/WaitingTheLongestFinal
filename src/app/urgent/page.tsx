import type { Metadata } from "next";
import Link from "next/link";

export const metadata: Metadata = {
  title: "Urgent Dogs - On Euthanasia Lists",
  description:
    "These shelter dogs are on euthanasia lists and running out of time. Real-time countdown timers show how long they have left. Share to save a life.",
  openGraph: {
    title: "Urgent Dogs - Euthanasia List Countdown | WaitingTheLongest.com",
    description:
      "These dogs are running out of time. View real-time countdown timers and help save their lives.",
  },
};

const URGENCY_SECTIONS = [
  {
    level: "critical" as const,
    title: "CRITICAL - Less Than 24 Hours",
    subtitle: "These dogs will be euthanized within 24 hours without intervention.",
    bgColor: "bg-red-50",
    borderColor: "border-red-300",
    headerBg: "bg-red-600",
    headerText: "text-white",
    dotColor: "bg-red-500",
    textColor: "text-red-800",
    cardBorder: "border-red-200",
    count: 0,
  },
  {
    level: "high" as const,
    title: "URGENT - Less Than 72 Hours",
    subtitle: "These dogs have less than 3 days before their scheduled euthanasia.",
    bgColor: "bg-orange-50",
    borderColor: "border-orange-300",
    headerBg: "bg-orange-500",
    headerText: "text-white",
    dotColor: "bg-orange-500",
    textColor: "text-orange-800",
    cardBorder: "border-orange-200",
    count: 0,
  },
  {
    level: "medium" as const,
    title: "AT RISK - Less Than 7 Days",
    subtitle: "These dogs have less than a week. Time is still critical.",
    bgColor: "bg-yellow-50",
    borderColor: "border-yellow-300",
    headerBg: "bg-yellow-500",
    headerText: "text-black",
    dotColor: "bg-yellow-500",
    textColor: "text-yellow-800",
    cardBorder: "border-yellow-200",
    count: 0,
  },
] as const;

const US_STATES = [
  "AL", "AK", "AZ", "AR", "CA", "CO", "CT", "DE", "FL", "GA",
  "HI", "ID", "IL", "IN", "IA", "KS", "KY", "LA", "ME", "MD",
  "MA", "MI", "MN", "MS", "MO", "MT", "NE", "NV", "NH", "NJ",
  "NM", "NY", "NC", "ND", "OH", "OK", "OR", "PA", "RI", "SC",
  "SD", "TN", "TX", "UT", "VT", "VA", "WA", "WV", "WI", "WY",
];

export default function UrgentPage() {
  return (
    <div>
      {/* Header */}
      <div className="bg-red-900 text-white">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-12">
          <div className="flex items-center gap-3 mb-4">
            <span className="inline-block w-4 h-4 bg-red-500 rounded-full animate-pulse" />
            <h1 className="text-3xl md:text-4xl font-bold">
              Urgent: Dogs on Euthanasia Lists
            </h1>
          </div>
          <p className="text-red-200 text-lg max-w-3xl mb-6">
            These dogs are scheduled to be euthanized. Each card has a real-time
            countdown timer showing how much time they have left. Sharing their
            profiles could literally save their lives.
          </p>
          <div className="flex flex-col sm:flex-row gap-3">
            <button className="px-6 py-3 bg-white text-red-900 font-bold rounded-lg hover:bg-red-50 transition">
              Share All Urgent Dogs
            </button>
            <div>
              <label htmlFor="state-filter" className="sr-only">
                Filter by state
              </label>
              <select
                id="state-filter"
                className="px-4 py-3 bg-red-800 border border-red-700 text-white rounded-lg focus:outline-none focus:ring-2 focus:ring-white"
                defaultValue=""
                disabled
              >
                <option value="">All States</option>
                {US_STATES.map((code) => (
                  <option key={code} value={code}>
                    {code}
                  </option>
                ))}
              </select>
            </div>
          </div>
        </div>
      </div>

      {/* Info Banner */}
      <div className="bg-red-100 border-b border-red-200">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-4">
          <div className="flex flex-col sm:flex-row items-start sm:items-center gap-3 text-sm text-red-800">
            <div className="flex items-center gap-2">
              <svg
                className="w-5 h-5 flex-shrink-0"
                fill="none"
                viewBox="0 0 24 24"
                stroke="currentColor"
              >
                <path
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  strokeWidth={2}
                  d="M13 16h-1v-4h-1m1-4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z"
                />
              </svg>
              <span>
                Euthanasia dates come directly from shelter records. Dates can
                change. Always verify with the shelter.
              </span>
            </div>
            <Link
              href="/foster"
              className="flex-shrink-0 font-semibold text-red-700 hover:text-red-900 underline"
            >
              Become an Emergency Foster
            </Link>
          </div>
        </div>
      </div>

      {/* Urgency Sections */}
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8 space-y-10">
        {URGENCY_SECTIONS.map((section) => (
          <section key={section.level}>
            {/* Section Header */}
            <div
              className={`${section.headerBg} ${section.headerText} rounded-t-xl px-6 py-4 flex items-center gap-3`}
            >
              <span
                className={`inline-block w-3 h-3 ${section.level === "critical" ? "bg-white animate-pulse" : "bg-white/70"} rounded-full`}
              />
              <div>
                <h2 className="text-xl font-bold">{section.title}</h2>
                <p className={`text-sm ${section.level === "medium" ? "text-black/70" : "text-white/80"}`}>
                  {section.subtitle}
                </p>
              </div>
            </div>

            {/* Section Content */}
            <div
              className={`${section.bgColor} ${section.borderColor} border border-t-0 rounded-b-xl p-6`}
            >
              {/* Placeholder Grid */}
              <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
                {Array.from({ length: 3 }).map((_, i) => (
                  <div
                    key={i}
                    className={`bg-white rounded-lg border ${section.cardBorder} overflow-hidden shadow-sm`}
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
                    <div className="p-4">
                      <div className="flex items-start justify-between mb-2">
                        <div>
                          <h3 className="font-bold text-gray-900">
                            Dog Name
                          </h3>
                          <p className="text-sm text-gray-600">Breed</p>
                        </div>
                        <span
                          className={`urgency-badge urgency-${section.level}`}
                        >
                          {section.level === "critical"
                            ? "CRITICAL"
                            : section.level === "high"
                              ? "URGENT"
                              : "AT RISK"}
                        </span>
                      </div>
                      <p className="text-xs text-gray-500 mb-3">
                        City, ST
                      </p>
                      {/* Countdown Placeholder */}
                      <div className="bg-gray-900 rounded px-2 py-2 text-center">
                        <p className="text-gray-500 text-xs font-mono">
                          CountdownTimer will display here
                        </p>
                      </div>
                    </div>
                  </div>
                ))}
              </div>

              {/* Empty State */}
              <div className="text-center py-8 mt-4">
                <p className={`${section.textColor} font-medium`}>
                  Connect Supabase to see {section.level} dogs
                </p>
                <p className="text-sm text-gray-500 mt-1">
                  Real-time countdown timers will appear on each card
                </p>
              </div>
            </div>
          </section>
        ))}
      </div>

      {/* Bottom CTA */}
      <div className="bg-gray-900 text-white">
        <div className="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8 py-12 text-center">
          <h2 className="text-2xl md:text-3xl font-bold mb-4">
            You Can Make the Difference
          </h2>
          <p className="text-gray-300 mb-8 max-w-2xl mx-auto">
            Sharing urgent dog profiles on social media can connect them with
            rescues, fosters, and adopters. A single share can save a life.
          </p>
          <div className="flex flex-col sm:flex-row gap-4 justify-center">
            <Link
              href="/foster"
              className="px-8 py-3 bg-green-500 hover:bg-green-600 text-white font-bold rounded-lg transition"
            >
              Become an Emergency Foster
            </Link>
            <button className="px-8 py-3 bg-white/10 hover:bg-white/20 border border-white/20 text-white font-bold rounded-lg transition">
              Share All Urgent Dogs
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
