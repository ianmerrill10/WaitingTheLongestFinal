import type { Metadata } from "next";
import Link from "next/link";
import { createClient } from "@/lib/supabase/server";

export const metadata: Metadata = {
  title: "Partner With Us | WaitingTheLongest.com",
  description:
    "List your shelter's dogs for free. Reach millions of adopters. Real-time sync, LED wait counters, and nationwide visibility — all at zero cost.",
  openGraph: {
    title: "List Your Dogs for Free | WaitingTheLongest.com",
    description:
      "Join 44,947+ organizations on America's largest real-time shelter dog platform. Free forever.",
  },
};

export default async function PartnersPage() {
  let shelterCount = 44947;
  let dogCount = 33233;
  let stateCount = 50;

  try {
    const supabase = await createClient();
    const [shelterResult, dogResult] = await Promise.all([
      supabase
        .from("shelters")
        .select("*", { count: "exact", head: true })
        .eq("is_active", true),
      supabase
        .from("dogs")
        .select("*", { count: "exact", head: true })
        .eq("is_available", true),
    ]);
    if (shelterResult.count) shelterCount = shelterResult.count;
    if (dogResult.count) dogCount = dogResult.count;
  } catch {
    // Use defaults
  }

  return (
    <div className="min-h-screen">
      {/* Hero Section */}
      <section className="bg-gradient-to-br from-green-700 via-green-800 to-green-900 text-white py-20 md:py-28">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 text-center">
          <h1 className="text-4xl md:text-6xl font-bold mb-6 leading-tight">
            List Your Dogs for Free.
            <br />
            <span className="text-led-green">Reach Millions of Adopters.</span>
          </h1>
          <p className="text-xl md:text-2xl text-green-100 max-w-3xl mx-auto mb-10">
            Join {shelterCount.toLocaleString()} organizations across all{" "}
            {stateCount} states on America&apos;s most urgent shelter dog
            platform. Free forever. No catches.
          </p>
          <div className="flex flex-col sm:flex-row gap-4 justify-center">
            <Link
              href="/partners/register"
              className="inline-flex items-center justify-center px-8 py-4 text-lg font-bold bg-white text-green-800 rounded-lg hover:bg-green-50 transition-colors shadow-lg"
            >
              Register Your Organization
            </Link>
            <a
              href="#how-it-works"
              className="inline-flex items-center justify-center px-8 py-4 text-lg font-semibold border-2 border-white text-white rounded-lg hover:bg-white/10 transition-colors"
            >
              Learn More
            </a>
          </div>
        </div>
      </section>

      {/* Stats Bar */}
      <section className="bg-gray-900 text-white py-8">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="grid grid-cols-1 md:grid-cols-3 gap-8 text-center">
            <div>
              <div className="text-3xl md:text-4xl font-bold text-led-green">
                {shelterCount.toLocaleString()}
              </div>
              <div className="text-gray-400 mt-1">
                Organizations Tracked
              </div>
            </div>
            <div>
              <div className="text-3xl md:text-4xl font-bold text-led-green">
                {dogCount.toLocaleString()}+
              </div>
              <div className="text-gray-400 mt-1">Dogs Listed</div>
            </div>
            <div>
              <div className="text-3xl md:text-4xl font-bold text-led-green">
                {stateCount}
              </div>
              <div className="text-gray-400 mt-1">States Covered</div>
            </div>
          </div>
        </div>
      </section>

      {/* Value Propositions */}
      <section className="py-16 md:py-24 bg-white">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <h2 className="text-3xl md:text-4xl font-bold text-center mb-16">
            Why Partner With WaitingTheLongest?
          </h2>
          <div className="grid grid-cols-1 md:grid-cols-3 gap-8">
            <ValueCard
              icon="$0"
              title="Free Forever"
              description="No fees, no premium tiers required, no hidden costs. Listing your dogs on our platform is and will always be completely free."
            />
            <ValueCard
              icon="LED"
              title="Real-Time Wait Counters"
              description="Every dog gets a live LED counter showing exactly how long they've been waiting. Critical dogs get countdown timers that create urgency."
            />
            <ValueCard
              icon="API"
              title="Seamless Integration"
              description="Connect via REST API, webhooks, CSV upload, or direct feed URL. Works with ShelterLuv, PetPoint, Chameleon, and more."
            />
            <ValueCard
              icon="50"
              title="Nationwide Reach"
              description="Your dogs are visible to adopters across all 50 states. Location-based search helps local adopters find your animals fast."
            />
            <ValueCard
              icon="RT"
              title="Real-Time Sync"
              description="Adopted? Updated? Status changes sync instantly. No stale listings, no dead links, no confusion for adopters."
            />
            <ValueCard
              icon="24/7"
              title="Always On"
              description="Our platform runs 24/7 with automated monitoring. Dogs are visible around the clock, maximizing adoption opportunities."
            />
          </div>
        </div>
      </section>

      {/* How It Works */}
      <section
        id="how-it-works"
        className="py-16 md:py-24 bg-gray-50"
      >
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <h2 className="text-3xl md:text-4xl font-bold text-center mb-16">
            How It Works
          </h2>
          <div className="grid grid-cols-1 md:grid-cols-3 gap-12 max-w-5xl mx-auto">
            <Step
              number={1}
              title="Register"
              description="Fill out a quick form with your organization's details. We'll match you against our database of 44,947+ known organizations."
            />
            <Step
              number={2}
              title="Integrate"
              description="Choose how to connect: REST API for real-time sync, webhook for event-driven updates, CSV upload for batch imports, or let us pull from your existing feed."
            />
            <Step
              number={3}
              title="Save Lives"
              description="Your dogs appear instantly with live wait time counters. Adopters find them through search, browse, and urgent alerts. Every view is a chance at a forever home."
            />
          </div>
        </div>
      </section>

      {/* Integration Options */}
      <section className="py-16 md:py-24 bg-white">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <h2 className="text-3xl md:text-4xl font-bold text-center mb-4">
            Integration Options
          </h2>
          <p className="text-lg text-gray-600 text-center mb-16 max-w-2xl mx-auto">
            Whether you&apos;re a tech-forward organization with a development
            team or a small rescue run from a kitchen table, we have an
            integration method that works for you.
          </p>
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
            <IntegrationCard
              title="REST API"
              description="Full programmatic control. Create, update, and manage dog listings via our documented API with per-key authentication."
              best="Organizations with IT staff"
            />
            <IntegrationCard
              title="Webhooks"
              description="Event-driven updates. Register an endpoint and we'll notify you when dogs are viewed, inquired about, or need attention."
              best="Shelter management systems"
            />
            <IntegrationCard
              title="Feed URL"
              description="Point us at your existing JSON or CSV feed. We'll sync automatically on a schedule you choose."
              best="Already have a public data feed"
            />
            <IntegrationCard
              title="CSV Upload"
              description="Upload a spreadsheet of your dogs. We'll map the columns and import them. Update anytime with a new file."
              best="Small rescues, any skill level"
            />
          </div>
        </div>
      </section>

      {/* FAQ Section */}
      <section className="py-16 md:py-24 bg-gray-50">
        <div className="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8">
          <h2 className="text-3xl md:text-4xl font-bold text-center mb-16">
            Frequently Asked Questions
          </h2>
          <div className="space-y-6">
            <FAQ
              q="Is it really free?"
              a="Yes. 100% free. No trial period, no premium upsell required for core features. Our mission is to save dogs, not sell subscriptions. We sustain the platform through ethical affiliate partnerships with pet supply companies."
            />
            <FAQ
              q="Will you process adoptions for us?"
              a="No. When an adopter is interested in one of your dogs, we redirect them to your website or provide your contact information. You maintain full control of the adoption process, screening, and decisions."
            />
            <FAQ
              q="What data do you need from us?"
              a="At minimum: dog name, breed, age, photo, and availability status. The more details you provide (medical info, temperament, good-with ratings), the better we can match dogs with adopters."
            />
            <FAQ
              q="We already use RescueGroups/ShelterLuv/PetPoint. Do we need to change?"
              a="Nope. Keep using whatever software you love. We can pull data from RescueGroups automatically, and we're adding direct integrations with ShelterLuv, PetPoint, and Chameleon."
            />
            <FAQ
              q="What about our dogs' data security?"
              a="Your data is encrypted in transit (TLS) and at rest. We never sell shelter contact information. You can request full data export or deletion at any time. See our Data Sharing Agreement for full details."
            />
            <FAQ
              q="Can we remove our dogs at any time?"
              a="Absolutely. You can deactivate individual listings instantly via API or request bulk removal. Upon termination, we retain data for 90 days then delete permanently — or delete immediately upon written request."
            />
          </div>
        </div>
      </section>

      {/* CTA */}
      <section className="bg-green-800 text-white py-16 md:py-20">
        <div className="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8 text-center">
          <h2 className="text-3xl md:text-4xl font-bold mb-6">
            Ready to Help Your Dogs Find Homes?
          </h2>
          <p className="text-xl text-green-100 mb-10 max-w-2xl mx-auto">
            Registration takes 5 minutes. Your dogs could be live on our
            platform today.
          </p>
          <Link
            href="/partners/register"
            className="inline-flex items-center justify-center px-10 py-4 text-lg font-bold bg-white text-green-800 rounded-lg hover:bg-green-50 transition-colors shadow-lg"
          >
            Register Your Organization Now
          </Link>
        </div>
      </section>
    </div>
  );
}

function ValueCard({
  icon,
  title,
  description,
}: {
  icon: string;
  title: string;
  description: string;
}) {
  return (
    <div className="bg-gray-50 rounded-xl p-8 text-center hover:shadow-lg transition-shadow">
      <div className="text-3xl font-bold text-green-700 mb-4 font-mono">
        {icon}
      </div>
      <h3 className="text-xl font-bold mb-3">{title}</h3>
      <p className="text-gray-600">{description}</p>
    </div>
  );
}

function Step({
  number,
  title,
  description,
}: {
  number: number;
  title: string;
  description: string;
}) {
  return (
    <div className="text-center">
      <div className="w-16 h-16 bg-green-700 text-white rounded-full flex items-center justify-center text-2xl font-bold mx-auto mb-6">
        {number}
      </div>
      <h3 className="text-xl font-bold mb-3">{title}</h3>
      <p className="text-gray-600">{description}</p>
    </div>
  );
}

function IntegrationCard({
  title,
  description,
  best,
}: {
  title: string;
  description: string;
  best: string;
}) {
  return (
    <div className="border border-gray-200 rounded-lg p-6 hover:border-green-400 hover:shadow-md transition-all">
      <h3 className="text-lg font-bold mb-2">{title}</h3>
      <p className="text-gray-600 text-sm mb-4">{description}</p>
      <div className="text-xs text-green-700 font-semibold uppercase tracking-wide">
        Best for: {best}
      </div>
    </div>
  );
}

function FAQ({ q, a }: { q: string; a: string }) {
  return (
    <div className="bg-white rounded-lg border border-gray-200 p-6">
      <h3 className="text-lg font-bold mb-2">{q}</h3>
      <p className="text-gray-600">{a}</p>
    </div>
  );
}
