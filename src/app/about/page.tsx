import type { Metadata } from "next";
import Link from "next/link";

export const metadata: Metadata = {
  title: "About Us - Our Mission",
  description:
    "WaitingTheLongest.com exists to shine a spotlight on shelter dogs who have been waiting the longest for a home. Learn about our mission, how we work, and how you can help.",
  openGraph: {
    title: "About WaitingTheLongest.com",
    description:
      "Our mission is to give visibility to overlooked shelter dogs and help them find loving homes.",
  },
};

export default function AboutPage() {
  return (
    <div>
      {/* Hero */}
      <div className="bg-wtl-dark text-white">
        <div className="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8 py-16 md:py-20 text-center">
          <h1 className="text-3xl md:text-5xl font-bold mb-4">
            Because Every Day{" "}
            <span className="text-led-green">Matters</span>
          </h1>
          <p className="text-xl text-gray-300 max-w-2xl mx-auto">
            WaitingTheLongest.com exists to shine a spotlight on the dogs that
            shelters struggle the most to place - the ones who have been waiting
            the longest, the ones on euthanasia lists, and the ones most people
            walk right past.
          </p>
        </div>
      </div>

      <div className="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8 py-12 space-y-16">
        {/* The Problem */}
        <section>
          <h2 className="text-2xl md:text-3xl font-bold text-gray-900 mb-6">
            The Problem
          </h2>
          <div className="prose prose-lg max-w-none text-gray-700 space-y-4">
            <p>
              Every year, approximately <strong>920,000 shelter animals are euthanized</strong> in
              the United States. While adoption rates have improved, millions of dogs
              still enter shelters annually, and many will never leave.
            </p>
            <p>
              The dogs most at risk are not the puppies or the photogenic breeds that
              get shared thousands of times on social media. The dogs most at risk are
              the ones nobody talks about: <strong>senior dogs</strong>,{" "}
              <strong>black dogs</strong>, <strong>pit bulls</strong>,{" "}
              <strong>bonded pairs</strong>, <strong>dogs with special needs</strong>,
              and <strong>dogs who have simply been waiting too long</strong>.
            </p>
            <p>
              These dogs become invisible. The longer they stay in a shelter, the less
              attention they receive, the more stressed they become, and the harder they
              are to adopt - a devastating cycle that often ends in euthanasia.
            </p>
          </div>
        </section>

        {/* Our Mission */}
        <section>
          <h2 className="text-2xl md:text-3xl font-bold text-gray-900 mb-6">
            Our Mission
          </h2>
          <div className="prose prose-lg max-w-none text-gray-700 space-y-4">
            <p>
              WaitingTheLongest.com was built on a simple idea:{" "}
              <strong>
                what if we sorted dogs not by cuteness or breed, but by how long
                they have been waiting?
              </strong>
            </p>
            <p>
              Every dog on our site has a real-time LED wait time counter showing
              exactly how long they have been in a shelter - down to the second. For
              dogs on euthanasia lists, a countdown timer shows how much time they
              have left. These are not abstract statistics. They are real dogs with
              real deadlines.
            </p>
            <p>Our goals are:</p>
          </div>
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 mt-6">
            <MissionCard
              number="01"
              title="Visibility for the Invisible"
              description="Surface the dogs that other platforms bury. The longest-waiting, most-overlooked dogs get top billing here."
            />
            <MissionCard
              number="02"
              title="Urgency That's Real"
              description="Real-time countdown timers for dogs on euthanasia lists. No sugarcoating. Every second counts."
            />
            <MissionCard
              number="03"
              title="Data-Driven Advocacy"
              description="Aggregate data from shelters nationwide to identify patterns, highlight at-risk populations, and drive change."
            />
            <MissionCard
              number="04"
              title="Foster Network Growth"
              description="Foster homes have 14x better adoption outcomes than shelters. We're building the tools to make fostering easier."
            />
          </div>
        </section>

        {/* How It Works */}
        <section>
          <h2 className="text-2xl md:text-3xl font-bold text-gray-900 mb-6">
            How It Works
          </h2>
          <div className="space-y-6">
            <StepItem
              step={1}
              title="Data Collection"
              description="We aggregate dog and shelter data from public APIs, shelter management systems, and partner organizations across all 50 states."
            />
            <StepItem
              step={2}
              title="Real-Time Tracking"
              description="Every dog gets a live wait time counter. Dogs flagged for euthanasia get countdown timers. Data is updated continuously."
            />
            <StepItem
              step={3}
              title="Smart Surfacing"
              description="Our default sort shows the longest-waiting dogs first. Urgent dogs get prominent placement. Overlooked categories get dedicated sections."
            />
            <StepItem
              step={4}
              title="Connection"
              description="When you find a dog you want to help, we connect you directly with their shelter. No middleman fees. Everything is free for shelters."
            />
          </div>
        </section>

        {/* Key Facts */}
        <section>
          <h2 className="text-2xl md:text-3xl font-bold text-gray-900 mb-6">
            Key Facts
          </h2>
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
            <FactCard
              value="~3.1M"
              label="Dogs enter shelters annually in the US"
            />
            <FactCard
              value="~920K"
              label="Shelter animals euthanized per year"
            />
            <FactCard value="14x" label="Better outcomes in foster homes vs. shelters" />
            <FactCard
              value="~40%"
              label="Of shelter dogs are pit bulls or mixes"
            />
            <FactCard value="25%" label="Adoption rate for senior dogs (vs 60% for puppies)" />
            <FactCard value="2x" label="Longer wait time for black dogs" />
          </div>
        </section>

        {/* Free for Shelters */}
        <section className="bg-green-50 rounded-2xl p-8 border border-green-200">
          <h2 className="text-2xl md:text-3xl font-bold text-green-900 mb-4">
            Free for Shelters. Always.
          </h2>
          <div className="text-green-800 space-y-3">
            <p>
              WaitingTheLongest.com will never charge shelters or rescue
              organizations to list their dogs. Our mission is to save lives, not
              make money from shelters that are already underfunded.
            </p>
            <p>
              We sustain ourselves through ethical affiliate partnerships (like
              Amazon pet supplies), optional premium features for adopters, and
              community donations. Shelters get everything for free.
            </p>
          </div>
        </section>

        {/* CTA */}
        <section className="text-center py-8">
          <h2 className="text-2xl md:text-3xl font-bold text-gray-900 mb-4">
            How You Can Help
          </h2>
          <p className="text-gray-600 max-w-xl mx-auto mb-8">
            Whether you adopt, foster, share, or donate - every action makes a
            difference. These dogs are counting on all of us.
          </p>
          <div className="flex flex-col sm:flex-row gap-4 justify-center">
            <Link
              href="/dogs"
              className="px-8 py-3 bg-blue-600 hover:bg-blue-700 text-white font-bold rounded-lg transition"
            >
              Browse Dogs
            </Link>
            <Link
              href="/foster"
              className="px-8 py-3 bg-green-600 hover:bg-green-700 text-white font-bold rounded-lg transition"
            >
              Become a Foster
            </Link>
            <Link
              href="/urgent"
              className="px-8 py-3 bg-red-600 hover:bg-red-700 text-white font-bold rounded-lg transition"
            >
              Help Urgent Dogs
            </Link>
          </div>
        </section>
      </div>
    </div>
  );
}

function MissionCard({
  number,
  title,
  description,
}: {
  number: string;
  title: string;
  description: string;
}) {
  return (
    <div className="bg-white rounded-xl p-6 border border-gray-200 shadow-sm">
      <span className="text-3xl font-bold text-blue-200">{number}</span>
      <h3 className="text-lg font-bold text-gray-900 mt-2 mb-2">{title}</h3>
      <p className="text-gray-600 text-sm">{description}</p>
    </div>
  );
}

function StepItem({
  step,
  title,
  description,
}: {
  step: number;
  title: string;
  description: string;
}) {
  return (
    <div className="flex gap-4">
      <div className="flex-shrink-0 w-10 h-10 bg-blue-600 text-white rounded-full flex items-center justify-center font-bold">
        {step}
      </div>
      <div>
        <h3 className="font-bold text-gray-900 mb-1">{title}</h3>
        <p className="text-gray-600">{description}</p>
      </div>
    </div>
  );
}

function FactCard({ value, label }: { value: string; label: string }) {
  return (
    <div className="bg-white rounded-xl p-5 border border-gray-200 text-center shadow-sm">
      <p className="text-3xl font-bold text-blue-600 mb-1">{value}</p>
      <p className="text-sm text-gray-600">{label}</p>
    </div>
  );
}
