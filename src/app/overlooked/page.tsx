import type { Metadata } from "next";
import Link from "next/link";
import { createClient } from "@/lib/supabase/server";
import DogGrid from "@/components/dogs/DogGrid";

export const metadata: Metadata = {
  title: "Overlooked Angels - Dogs That Need Extra Love",
  description:
    "Senior dogs, black dogs, pit bulls, bonded pairs, special needs dogs, and long-term residents are statistically most likely to be euthanized. Learn why and help them find homes.",
  openGraph: {
    title: "Overlooked Angels | WaitingTheLongest.com",
    description:
      "These dogs are the hardest to adopt out. Senior dogs, black dogs, pit bulls, and more need your help.",
  },
};

const SECTIONS = [
  {
    id: "senior-dogs",
    title: "Senior Dogs",
    subtitle: "7+ years of love to give",
    bgColor: "bg-amber-50",
    borderColor: "border-amber-200",
    accentColor: "text-amber-800",
    iconBg: "bg-amber-100",
    description:
      "Senior dogs are among the hardest to adopt from shelters. Many people overlook them in favor of puppies, but senior dogs make incredible companions. They are often already house-trained, calmer, and deeply grateful for a second chance. Studies show senior dogs bond just as strongly with new owners, and their needs are often more predictable than younger dogs.",
    stats: [
      { label: "Adoption rate for seniors", value: "25%", subtext: "vs. 60% for puppies" },
      { label: "Average shelter stay", value: "4x longer", subtext: "than puppies" },
      { label: "Return rate", value: "Lowest", subtext: "of any age group" },
    ],
    cta: "Senior dogs ask for so little and give so much. They don't need years of training - they just need a warm bed and someone to love.",
  },
  {
    id: "black-dogs",
    title: "Black Dogs",
    subtitle: "Overlooked but beautiful",
    bgColor: "bg-gray-50",
    borderColor: "border-gray-200",
    accentColor: "text-gray-800",
    iconBg: "bg-gray-100",
    description:
      "Black Dog Syndrome is a well-documented phenomenon in animal shelters. Black dogs are consistently passed over for lighter-colored dogs, partially due to how they photograph (they tend to look less expressive in photos) and unconscious biases. In reality, a dog's coat color has zero correlation with temperament, health, or trainability.",
    stats: [
      { label: "Time in shelter", value: "2x longer", subtext: "than lighter-colored dogs" },
      { label: "Photo engagement", value: "30% lower", subtext: "on adoption sites" },
      { label: "Euthanasia risk", value: "Significantly higher", subtext: "in high-intake shelters" },
    ],
    cta: "A dog's color says nothing about their heart. Black dogs are just as loving, loyal, and playful as any other dog.",
  },
  {
    id: "pit-bulls",
    title: "Pit Bulls & Bully Breeds",
    subtitle: "Misunderstood and loyal",
    bgColor: "bg-blue-50",
    borderColor: "border-blue-200",
    accentColor: "text-blue-800",
    iconBg: "bg-blue-100",
    description:
      "Pit bulls and bully breeds make up the largest percentage of shelter dogs in America, and they face the highest euthanasia rates. Breed-specific legislation, housing restrictions, and widespread misconceptions make them nearly impossible to place. The American Veterinary Medical Association has stated that breed is a poor predictor of individual dog behavior. These dogs are often incredibly loyal, affectionate, and eager to please.",
    stats: [
      { label: "Shelter population", value: "~40%", subtext: "of all shelter dogs" },
      { label: "Euthanasia rate", value: "Highest", subtext: "of any breed group" },
      { label: "Temperament test pass rate", value: "87%", subtext: "higher than many popular breeds" },
    ],
    cta: "Pit bulls were once known as 'nanny dogs' for their gentleness with children. They deserve to be judged as individuals, not by their breed.",
  },
  {
    id: "bonded-pairs",
    title: "Bonded Pairs",
    subtitle: "Better together, harder to place",
    bgColor: "bg-pink-50",
    borderColor: "border-pink-200",
    accentColor: "text-pink-800",
    iconBg: "bg-pink-100",
    description:
      "Bonded pairs are two dogs that have formed a deep emotional connection and suffer severe distress when separated. Shelters try to keep them together, but finding a home willing to adopt two dogs at once is extremely difficult. Separating bonded pairs can cause depression, anxiety, refusal to eat, and behavioral problems in both dogs.",
    stats: [
      { label: "Adoption wait time", value: "3-5x longer", subtext: "than single dogs" },
      { label: "Separation anxiety rate", value: "Very high", subtext: "when split apart" },
      { label: "Adopter benefit", value: "2 dogs, 1 adjustment", subtext: "they comfort each other" },
    ],
    cta: "When you adopt a bonded pair, you get two best friends who already love each other - and they'll love you twice as much.",
  },
  {
    id: "special-needs",
    title: "Special Needs Dogs",
    subtitle: "Extra special love",
    bgColor: "bg-purple-50",
    borderColor: "border-purple-200",
    accentColor: "text-purple-800",
    iconBg: "bg-purple-100",
    description:
      "Dogs with disabilities, chronic conditions, or behavioral challenges face enormous obstacles in finding homes. Whether they're deaf, blind, missing a limb, managing diabetes, or recovering from abuse, these dogs adapt remarkably well and live happy, fulfilling lives. Many special needs are very manageable with basic veterinary care.",
    stats: [
      { label: "Adoption rate", value: "Under 10%", subtext: "in most shelters" },
      { label: "Euthanasia risk", value: "Extremely high", subtext: "especially in crowded shelters" },
      { label: "Quality of life", value: "Excellent", subtext: "with proper care" },
    ],
    cta: "Special needs dogs don't know they're different. They play, love, and live with the same joy as any other dog. They just need someone to give them a chance.",
  },
  {
    id: "long-timers",
    title: "Long-Term Residents",
    subtitle: "6+ months and still waiting",
    bgColor: "bg-teal-50",
    borderColor: "border-teal-200",
    accentColor: "text-teal-800",
    iconBg: "bg-teal-100",
    description:
      "Some dogs spend months or even years in shelters. The longer they stay, the less visible they become. New arrivals get attention while long-term residents are pushed to the back. Shelter environments can also cause behavioral deterioration over time, making these dogs even harder to adopt - a vicious cycle. Many long-term residents are perfectly wonderful dogs who simply got unlucky.",
    stats: [
      { label: "Average long-term stay", value: "6-18 months", subtext: "some exceed 2+ years" },
      { label: "Behavioral decline", value: "Documented", subtext: "after 3+ months in shelter" },
      { label: "Post-adoption behavior", value: "Dramatic improvement", subtext: "in home environment" },
    ],
    cta: "The longest waiting dogs are often the most grateful. Given a chance, they bloom into incredible companions. They just need someone to see them.",
  },
];

// Fetch dogs per category from Supabase
async function fetchOverlookedDogs() {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const categories: Record<string, any[]> = {};
  try {
    const supabase = await createClient();

    // Senior dogs
    const { data: seniors } = await supabase
      .from("dogs")
      .select("*, shelters!inner(name, city, state_code)")
      .eq("is_available", true)
      .eq("age_category", "senior")
      .order("intake_date", { ascending: true })
      .limit(4);
    categories["senior-dogs"] = seniors || [];

    // Special needs dogs
    const { data: specialNeeds } = await supabase
      .from("dogs")
      .select("*, shelters!inner(name, city, state_code)")
      .eq("is_available", true)
      .eq("has_special_needs", true)
      .order("intake_date", { ascending: true })
      .limit(4);
    categories["special-needs"] = specialNeeds || [];

    // Long-term residents (intake > 6 months ago)
    const sixMonthsAgo = new Date();
    sixMonthsAgo.setMonth(sixMonthsAgo.getMonth() - 6);
    const { data: longTimers } = await supabase
      .from("dogs")
      .select("*, shelters!inner(name, city, state_code)")
      .eq("is_available", true)
      .lt("intake_date", sixMonthsAgo.toISOString())
      .order("intake_date", { ascending: true })
      .limit(4);
    categories["long-timers"] = longTimers || [];

    // Pit bulls / bully breeds
    const { data: pitBulls } = await supabase
      .from("dogs")
      .select("*, shelters!inner(name, city, state_code)")
      .eq("is_available", true)
      .or("breed_primary.ilike.%pit bull%,breed_primary.ilike.%american staffordshire%,breed_primary.ilike.%staffordshire bull%,breed_primary.ilike.%bull terrier%")
      .order("intake_date", { ascending: true })
      .limit(4);
    categories["pit-bulls"] = pitBulls || [];

    // Black dogs (search color_primary or breed description containing "black")
    const { data: blackDogs } = await supabase
      .from("dogs")
      .select("*, shelters!inner(name, city, state_code)")
      .eq("is_available", true)
      .or("color_primary.ilike.%black%,breed_primary.ilike.%black%")
      .order("intake_date", { ascending: true })
      .limit(4);
    categories["black-dogs"] = blackDogs || [];

    // Bonded pairs (search description for "bonded" keyword)
    const { data: bondedPairs } = await supabase
      .from("dogs")
      .select("*, shelters!inner(name, city, state_code)")
      .eq("is_available", true)
      .or("description.ilike.%bonded pair%,description.ilike.%bonded with%,description.ilike.%must be adopted together%,tags.cs.{bonded}")
      .order("intake_date", { ascending: true })
      .limit(4);
    categories["bonded-pairs"] = bondedPairs || [];
  } catch {
    // Supabase not configured yet
  }
  return categories;
}

export default async function OverlookedPage() {
  const categoryDogs = await fetchOverlookedDogs();
  return (
    <div>
      {/* Hero Header */}
      <div className="bg-purple-900 text-white">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-12 md:py-16">
          <h1 className="text-3xl md:text-5xl font-bold mb-4">
            Overlooked Angels
          </h1>
          <p className="text-purple-200 text-lg md:text-xl max-w-3xl mb-6">
            Some dogs are statistically far more likely to be euthanized - not
            because anything is wrong with them, but because of misconceptions,
            biases, and circumstances beyond their control.
          </p>
          <p className="text-purple-300 max-w-2xl">
            Learn about these overlooked categories and consider giving one of
            these incredible dogs a chance. They might just be the best decision
            you ever make.
          </p>
        </div>
      </div>

      {/* Quick Navigation */}
      <div className="bg-white border-b border-gray-200 sticky top-16 z-40">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <nav className="flex gap-1 overflow-x-auto py-3 -mb-px" aria-label="Section navigation">
            {SECTIONS.map((section) => (
              <a
                key={section.id}
                href={`#${section.id}`}
                className="flex-shrink-0 px-3 py-1.5 text-sm font-medium text-gray-600 hover:text-purple-700 hover:bg-purple-50 rounded-full transition"
              >
                {section.title}
              </a>
            ))}
          </nav>
        </div>
      </div>

      {/* Sections */}
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-10 space-y-12">
        {SECTIONS.map((section) => (
          <section
            key={section.id}
            id={section.id}
            className={`rounded-2xl ${section.bgColor} border ${section.borderColor} overflow-hidden scroll-mt-28`}
          >
            {/* Section Header */}
            <div className="px-6 md:px-8 pt-8 pb-4">
              <h2
                className={`text-2xl md:text-3xl font-bold ${section.accentColor} mb-1`}
              >
                {section.title}
              </h2>
              <p className="text-gray-600">{section.subtitle}</p>
            </div>

            {/* Description */}
            <div className="px-6 md:px-8 pb-6">
              <p className="text-gray-700 leading-relaxed max-w-4xl">
                {section.description}
              </p>
            </div>

            {/* Stats */}
            <div className="px-6 md:px-8 pb-6">
              <div className="grid grid-cols-1 sm:grid-cols-3 gap-4">
                {section.stats.map((stat) => (
                  <div
                    key={stat.label}
                    className="bg-white/80 rounded-lg p-4 text-center"
                  >
                    <p className="text-xs text-gray-500 uppercase tracking-wide mb-1">
                      {stat.label}
                    </p>
                    <p className={`text-xl font-bold ${section.accentColor}`}>
                      {stat.value}
                    </p>
                    <p className="text-xs text-gray-500 mt-0.5">
                      {stat.subtext}
                    </p>
                  </div>
                ))}
              </div>
            </div>

            {/* Dog Cards */}
            <div className="px-6 md:px-8 pb-6">
              <h3 className="font-semibold text-gray-900 mb-3">
                {section.title} Available for Adoption
              </h3>
              {categoryDogs[section.id] && categoryDogs[section.id].length > 0 ? (
                <DogGrid dogs={categoryDogs[section.id]} emptyMessage={`No ${section.title.toLowerCase()} currently available.`} />
              ) : (
                <div className="text-center py-8 bg-white/50 rounded-lg">
                  <p className="text-sm text-gray-500">
                    {section.title} available for adoption will appear here once
                    the database is connected and synced.
                  </p>
                </div>
              )}
            </div>

            {/* Quote / CTA */}
            <div className="px-6 md:px-8 pb-8">
              <blockquote
                className={`border-l-4 ${section.borderColor} pl-4 italic text-gray-600`}
              >
                {section.cta}
              </blockquote>
            </div>
          </section>
        ))}
      </div>

      {/* Bottom CTA */}
      <div className="bg-purple-600 text-white">
        <div className="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8 py-12 text-center">
          <h2 className="text-2xl md:text-3xl font-bold mb-4">
            Give an Overlooked Angel a Chance
          </h2>
          <p className="text-purple-200 mb-8 max-w-2xl mx-auto">
            These dogs have so much love to give. Whether you adopt, foster, or
            share their stories, you can help save a life that others have
            passed by.
          </p>
          <div className="flex flex-col sm:flex-row gap-4 justify-center">
            <Link
              href="/dogs"
              className="px-8 py-3 bg-white text-purple-700 font-bold rounded-lg hover:bg-purple-50 transition"
            >
              Browse All Dogs
            </Link>
            <Link
              href="/foster"
              className="px-8 py-3 bg-purple-800 hover:bg-purple-900 text-white font-bold rounded-lg transition border border-purple-500"
            >
              Become a Foster
            </Link>
          </div>
        </div>
      </div>
    </div>
  );
}
