import type { Metadata } from "next";
import Link from "next/link";

export const metadata: Metadata = {
  title: "Become a Foster - Save a Life",
  description:
    "Foster homes have 14x better adoption outcomes than shelters. Learn how fostering works, why it matters, and sign up to become a foster hero.",
  openGraph: {
    title: "Become a Foster Hero | WaitingTheLongest.com",
    description:
      "Foster a shelter dog and give them 14x better adoption chances. Sign up today.",
  },
};

export default function FosterPage() {
  return (
    <div>
      {/* Hero */}
      <div className="bg-green-700 text-white">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-16 md:py-20">
          <div className="max-w-3xl">
            <h1 className="text-3xl md:text-5xl font-bold mb-4">
              Become a Foster Hero
            </h1>
            <p className="text-xl text-green-100 mb-6">
              You don&apos;t have to adopt to save a life. Fostering a dog - even
              temporarily - gives them <strong>14x better chances</strong> of
              finding a forever home.
            </p>
            <div className="flex flex-col sm:flex-row gap-3">
              <a
                href="#signup"
                className="inline-block px-8 py-3 bg-white text-green-700 font-bold rounded-lg hover:bg-green-50 transition text-center"
              >
                Sign Up to Foster
              </a>
              <a
                href="#how-it-works"
                className="inline-block px-8 py-3 bg-green-600 hover:bg-green-800 text-white font-bold rounded-lg transition border border-green-500 text-center"
              >
                Learn How It Works
              </a>
            </div>
          </div>
        </div>
      </div>

      {/* Stats Banner */}
      <div className="bg-green-600 text-white border-t border-green-500">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
          <div className="grid grid-cols-2 md:grid-cols-4 gap-6 text-center">
            <div>
              <p className="text-3xl md:text-4xl font-bold">14x</p>
              <p className="text-green-200 text-sm mt-1">
                Better adoption outcomes
              </p>
            </div>
            <div>
              <p className="text-3xl md:text-4xl font-bold">2-4 wks</p>
              <p className="text-green-200 text-sm mt-1">
                Average foster period
              </p>
            </div>
            <div>
              <p className="text-3xl md:text-4xl font-bold">$0</p>
              <p className="text-green-200 text-sm mt-1">
                Cost to you (supplies provided)
              </p>
            </div>
            <div>
              <p className="text-3xl md:text-4xl font-bold">1</p>
              <p className="text-green-200 text-sm mt-1">
                Life saved by you
              </p>
            </div>
          </div>
        </div>
      </div>

      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-12">
        {/* Why Foster Section */}
        <section className="mb-16">
          <h2 className="text-2xl md:text-3xl font-bold text-gray-900 mb-8 text-center">
            Why Fostering Changes Everything
          </h2>
          <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
            <BenefitCard
              title="For the Dog"
              items={[
                "Escapes the stress of shelter environments",
                "Gets individual attention and socialization",
                "Develops better behavior in a home setting",
                "Medical and emotional recovery improves dramatically",
                "14x more likely to be adopted",
              ]}
              accentColor="border-green-500"
              iconBg="bg-green-100"
              iconColor="text-green-600"
            />
            <BenefitCard
              title="For the Shelter"
              items={[
                "Frees up kennel space for incoming animals",
                "Reduces euthanasia numbers",
                "Gets better profiles/photos of dogs",
                "Learns dog's true personality for better matching",
                "Lower operational costs per animal",
              ]}
              accentColor="border-blue-500"
              iconBg="bg-blue-100"
              iconColor="text-blue-600"
            />
            <BenefitCard
              title="For You"
              items={[
                "Companionship without long-term commitment",
                "Try having a dog before committing to adoption",
                "Feel-good impact: you literally saved a life",
                "Supplies and vet care typically covered",
                "Join a community of fellow foster heroes",
              ]}
              accentColor="border-purple-500"
              iconBg="bg-purple-100"
              iconColor="text-purple-600"
            />
          </div>
        </section>

        {/* How It Works */}
        <section id="how-it-works" className="mb-16 scroll-mt-20">
          <h2 className="text-2xl md:text-3xl font-bold text-gray-900 mb-8 text-center">
            How Fostering Works
          </h2>
          <div className="max-w-3xl mx-auto space-y-8">
            <StepCard
              step={1}
              title="Sign Up"
              description="Fill out a short application telling us about your home, experience with dogs, and preferences (size, energy level, etc.). No experience necessary - shelters provide training and support."
            />
            <StepCard
              step={2}
              title="Get Matched"
              description="Based on your profile, we connect you with a local shelter that has a dog matching your preferences. You'll meet the dog and make sure it's a good fit."
            />
            <StepCard
              step={3}
              title="Welcome Home"
              description="Take your foster dog home! The shelter provides food, supplies, and veterinary care. You provide love, a couch, and a safe space."
            />
            <StepCard
              step={4}
              title="Help Them Shine"
              description="Take photos, share updates, and let their true personality come through. Dogs in foster homes photograph and present better, dramatically boosting adoption chances."
            />
            <StepCard
              step={5}
              title="Happy Ending"
              description="When your foster dog gets adopted, celebrate knowing you played a crucial role in saving their life. Then, if you're ready, do it again."
            />
          </div>
        </section>

        {/* FAQ */}
        <section className="mb-16">
          <h2 className="text-2xl md:text-3xl font-bold text-gray-900 mb-8 text-center">
            Common Questions
          </h2>
          <div className="max-w-3xl mx-auto space-y-4">
            <FAQItem
              question="How long does fostering last?"
              answer="It varies. Emergency fosters can be as short as a weekend. Standard fosters are typically 2-4 weeks, sometimes longer for dogs recovering from surgery or illness. You set your availability."
            />
            <FAQItem
              question="What if I get too attached?"
              answer="This is common and completely understandable - it's called being a 'foster fail'! Many fosters end up adopting their foster dogs. But remember: even if it's hard to let go, you're freeing up space to save another life."
            />
            <FAQItem
              question="Who pays for food and vet bills?"
              answer="Most shelters cover all costs including food, supplies, and veterinary care. You provide the home and the love. Check with your local shelter for specific details."
            />
            <FAQItem
              question="Can I foster if I work full time?"
              answer="Absolutely! Most dogs are fine being home alone during work hours, especially adults and seniors. Puppies and dogs with special needs may require more time, but there are dogs for every lifestyle."
            />
            <FAQItem
              question="What if the dog has behavioral issues?"
              answer="Shelters will match you with a dog appropriate for your experience level. If issues arise, the shelter provides behavioral support and training resources. You are never alone in this."
            />
            <FAQItem
              question="I already have pets. Can I still foster?"
              answer="Yes! Many foster dogs are good with other animals. Shelters will help match you with a compatible dog and provide guidance on introductions."
            />
          </div>
        </section>

        {/* Sign Up Form Placeholder */}
        <section id="signup" className="scroll-mt-20">
          <div className="max-w-2xl mx-auto bg-white rounded-2xl shadow-lg border border-gray-200 overflow-hidden">
            <div className="bg-green-600 text-white p-6">
              <h2 className="text-2xl font-bold">Foster Application</h2>
              <p className="text-green-100 mt-1">
                Fill out this form and we will connect you with a local shelter.
              </p>
            </div>
            <div className="p-6 space-y-5">
              <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
                <div>
                  <label
                    htmlFor="first-name"
                    className="block text-sm font-medium text-gray-700 mb-1"
                  >
                    First Name
                  </label>
                  <input
                    id="first-name"
                    type="text"
                    className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 focus:border-transparent"
                    disabled
                  />
                </div>
                <div>
                  <label
                    htmlFor="last-name"
                    className="block text-sm font-medium text-gray-700 mb-1"
                  >
                    Last Name
                  </label>
                  <input
                    id="last-name"
                    type="text"
                    className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 focus:border-transparent"
                    disabled
                  />
                </div>
              </div>

              <div>
                <label
                  htmlFor="email"
                  className="block text-sm font-medium text-gray-700 mb-1"
                >
                  Email Address
                </label>
                <input
                  id="email"
                  type="email"
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 focus:border-transparent"
                  disabled
                />
              </div>

              <div>
                <label
                  htmlFor="zip"
                  className="block text-sm font-medium text-gray-700 mb-1"
                >
                  Zip Code
                </label>
                <input
                  id="zip"
                  type="text"
                  className="w-full max-w-[200px] px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 focus:border-transparent"
                  disabled
                />
              </div>

              <div>
                <label
                  htmlFor="housing"
                  className="block text-sm font-medium text-gray-700 mb-1"
                >
                  Housing Type
                </label>
                <select
                  id="housing"
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 focus:border-transparent"
                  disabled
                >
                  <option>Select...</option>
                  <option>House with yard</option>
                  <option>House without yard</option>
                  <option>Apartment/Condo</option>
                  <option>Townhouse</option>
                  <option>Other</option>
                </select>
              </div>

              <div>
                <label
                  htmlFor="experience"
                  className="block text-sm font-medium text-gray-700 mb-1"
                >
                  Dog Experience
                </label>
                <select
                  id="experience"
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 focus:border-transparent"
                  disabled
                >
                  <option>Select...</option>
                  <option>First-time dog owner</option>
                  <option>Some experience</option>
                  <option>Experienced dog owner</option>
                  <option>Professional / trainer</option>
                </select>
              </div>

              <div>
                <label
                  htmlFor="size-pref"
                  className="block text-sm font-medium text-gray-700 mb-1"
                >
                  Preferred Dog Size
                </label>
                <select
                  id="size-pref"
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 focus:border-transparent"
                  disabled
                >
                  <option>Any size</option>
                  <option>Small (under 25 lbs)</option>
                  <option>Medium (25-50 lbs)</option>
                  <option>Large (50-90 lbs)</option>
                  <option>Extra Large (90+ lbs)</option>
                </select>
              </div>

              <div>
                <label
                  htmlFor="notes"
                  className="block text-sm font-medium text-gray-700 mb-1"
                >
                  Anything Else We Should Know?
                </label>
                <textarea
                  id="notes"
                  rows={3}
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 focus:border-transparent"
                  disabled
                />
              </div>

              <div className="pt-2">
                <button
                  className="w-full py-3 px-6 bg-green-600 text-white font-bold rounded-lg transition cursor-not-allowed opacity-75"
                  disabled
                >
                  Submit Application
                </button>
                <p className="text-xs text-gray-500 text-center mt-3">
                  Foster applications will be processed once the system is
                  fully connected. Your information will be shared with local
                  shelters in your area.
                </p>
              </div>
            </div>
          </div>
        </section>

        {/* Emergency Foster CTA */}
        <section className="mt-16 bg-red-50 rounded-2xl p-8 border border-red-200 text-center">
          <h2 className="text-2xl font-bold text-red-900 mb-3">
            Emergency Foster Needed?
          </h2>
          <p className="text-red-700 max-w-xl mx-auto mb-6">
            Some dogs need immediate foster placement to avoid euthanasia. If you
            can take a dog in on short notice, even for a few days, you could save
            a life this week.
          </p>
          <Link
            href="/urgent"
            className="inline-block px-8 py-3 bg-red-600 hover:bg-red-700 text-white font-bold rounded-lg transition"
          >
            View Dogs That Need Emergency Fosters
          </Link>
        </section>
      </div>
    </div>
  );
}

function BenefitCard({
  title,
  items,
  accentColor,
  iconBg,
  iconColor,
}: {
  title: string;
  items: string[];
  accentColor: string;
  iconBg: string;
  iconColor: string;
}) {
  return (
    <div className={`bg-white rounded-xl p-6 border-t-4 ${accentColor} shadow-sm border border-gray-200`}>
      <h3 className="text-xl font-bold text-gray-900 mb-4">{title}</h3>
      <ul className="space-y-3">
        {items.map((item) => (
          <li key={item} className="flex items-start gap-2">
            <span
              className={`flex-shrink-0 w-5 h-5 ${iconBg} ${iconColor} rounded-full flex items-center justify-center mt-0.5`}
            >
              <svg
                className="w-3 h-3"
                fill="none"
                viewBox="0 0 24 24"
                stroke="currentColor"
                strokeWidth={3}
              >
                <path
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  d="M5 13l4 4L19 7"
                />
              </svg>
            </span>
            <span className="text-sm text-gray-700">{item}</span>
          </li>
        ))}
      </ul>
    </div>
  );
}

function StepCard({
  step,
  title,
  description,
}: {
  step: number;
  title: string;
  description: string;
}) {
  return (
    <div className="flex gap-5">
      <div className="flex-shrink-0 w-12 h-12 bg-green-600 text-white rounded-full flex items-center justify-center font-bold text-lg">
        {step}
      </div>
      <div>
        <h3 className="text-lg font-bold text-gray-900 mb-1">{title}</h3>
        <p className="text-gray-600">{description}</p>
      </div>
    </div>
  );
}

function FAQItem({
  question,
  answer,
}: {
  question: string;
  answer: string;
}) {
  return (
    <div className="bg-white rounded-lg border border-gray-200 p-5">
      <h3 className="font-bold text-gray-900 mb-2">{question}</h3>
      <p className="text-gray-600 text-sm leading-relaxed">{answer}</p>
    </div>
  );
}
