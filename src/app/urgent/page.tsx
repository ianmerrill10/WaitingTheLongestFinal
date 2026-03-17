import type { Metadata } from "next";
import Link from "next/link";
import { createClient } from "@/lib/supabase/server";
import DogGrid from "@/components/dogs/DogGrid";

export const metadata: Metadata = {
  title: "Urgent Dogs - On Euthanasia Lists",
  description:
    "These shelter dogs are on euthanasia lists and running out of time. Real-time countdown timers show how long they have left.",
};

const URGENCY_SECTIONS = [
  {
    level: "critical",
    title: "CRITICAL - Less Than 24 Hours",
    subtitle: "These dogs will be euthanized within 24 hours without intervention.",
    bgColor: "bg-red-50",
    borderColor: "border-red-300",
    headerBg: "bg-red-600",
    headerText: "text-white",
    textColor: "text-red-800",
  },
  {
    level: "high",
    title: "URGENT - Less Than 72 Hours",
    subtitle: "These dogs have less than 3 days before their scheduled euthanasia.",
    bgColor: "bg-orange-50",
    borderColor: "border-orange-300",
    headerBg: "bg-orange-500",
    headerText: "text-white",
    textColor: "text-orange-800",
  },
  {
    level: "medium",
    title: "AT RISK - Less Than 7 Days",
    subtitle: "These dogs have less than a week. Time is still critical.",
    bgColor: "bg-yellow-50",
    borderColor: "border-yellow-300",
    headerBg: "bg-yellow-500",
    headerText: "text-black",
    textColor: "text-yellow-800",
  },
] as const;

export default async function UrgentPage() {
  const supabase = await createClient();

  // Fetch dogs for each urgency level
  const results = await Promise.all(
    URGENCY_SECTIONS.map((section) =>
      supabase
        .from("dogs")
        .select("*, shelters!inner(name, city, state_code)")
        .eq("is_available", true)
        .eq("urgency_level", section.level)
        .order("euthanasia_date", { ascending: true, nullsFirst: false })
        .limit(12)
    )
  );

  const sectionData = URGENCY_SECTIONS.map((section, i) => ({
    ...section,
    dogs: results[i].data || [],
  }));

  const totalUrgent = sectionData.reduce((sum, s) => sum + s.dogs.length, 0);

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
            {totalUrgent > 0
              ? `${totalUrgent} dogs are currently at risk. Each card has a real-time countdown timer. Sharing their profiles could literally save their lives.`
              : "When dogs are on euthanasia lists, they'll appear here with real-time countdown timers. Sharing could save a life."}
          </p>
          <div className="flex flex-col sm:flex-row gap-3">
            <Link
              href="/foster"
              className="px-6 py-3 bg-white text-red-900 font-bold rounded-lg hover:bg-red-50 transition text-center"
            >
              Become an Emergency Foster
            </Link>
          </div>
        </div>
      </div>

      {/* Info Banner */}
      <div className="bg-red-100 border-b border-red-200">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-4">
          <div className="flex flex-col sm:flex-row items-start sm:items-center gap-3 text-sm text-red-800">
            <div className="flex items-center gap-2">
              <svg className="w-5 h-5 flex-shrink-0" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M13 16h-1v-4h-1m1-4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
              </svg>
              <span>
                Euthanasia dates come from shelter records and AI monitoring. Always verify with the shelter.
              </span>
            </div>
          </div>
        </div>
      </div>

      {/* Urgency Sections */}
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8 space-y-10">
        {sectionData.map((section) => (
          <section key={section.level}>
            <div className={`${section.headerBg} ${section.headerText} rounded-t-xl px-6 py-4 flex items-center gap-3`}>
              <span className={`inline-block w-3 h-3 ${section.level === "critical" ? "bg-white animate-pulse" : "bg-white/70"} rounded-full`} />
              <div>
                <h2 className="text-xl font-bold">{section.title}</h2>
                <p className={`text-sm ${section.level === "medium" ? "text-black/70" : "text-white/80"}`}>
                  {section.subtitle}
                </p>
              </div>
              {section.dogs.length > 0 && (
                <span className="ml-auto bg-white/20 px-3 py-1 rounded-full text-sm font-bold">
                  {section.dogs.length}
                </span>
              )}
            </div>

            <div className={`${section.bgColor} ${section.borderColor} border border-t-0 rounded-b-xl p-6`}>
              {section.dogs.length > 0 ? (
                <DogGrid dogs={section.dogs} showCountdown urgencyHighlight />
              ) : (
                <div className="text-center py-8">
                  <p className={`${section.textColor} font-medium`}>
                    No {section.level} dogs right now
                  </p>
                  <p className="text-sm text-gray-500 mt-1">
                    Real-time countdown timers will appear when dogs are flagged
                  </p>
                </div>
              )}
            </div>
          </section>
        ))}
      </div>

      {/* Bottom CTA */}
      <div className="bg-gray-900 text-white">
        <div className="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8 py-12 text-center">
          <h2 className="text-2xl md:text-3xl font-bold mb-4">You Can Make the Difference</h2>
          <p className="text-gray-300 mb-8 max-w-2xl mx-auto">
            Sharing urgent dog profiles on social media can connect them with
            rescues, fosters, and adopters. A single share can save a life.
          </p>
          <div className="flex flex-col sm:flex-row gap-4 justify-center">
            <Link href="/foster" className="px-8 py-3 bg-green-500 hover:bg-green-600 text-white font-bold rounded-lg transition">
              Become an Emergency Foster
            </Link>
          </div>
        </div>
      </div>
    </div>
  );
}
