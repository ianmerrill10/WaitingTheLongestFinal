import type { Metadata } from "next";
import Link from "next/link";
import LEDCounter from "@/components/counters/LEDCounter";
import CountdownTimer from "@/components/counters/CountdownTimer";
import UrgencyBadge from "@/components/ui/UrgencyBadge";

export async function generateMetadata({
  params,
}: {
  params: Promise<{ id: string }>;
}): Promise<Metadata> {
  const { id } = await params;
  return {
    title: `Dog Profile - ${id}`,
    description:
      "View this adoptable shelter dog's full profile, including wait time, medical info, and how to adopt. Every day matters.",
    openGraph: {
      title: `Adoptable Dog | WaitingTheLongest.com`,
      description:
        "This shelter dog is waiting for a loving home. See their full profile and find out how to help.",
    },
  };
}

// Placeholder data representing what a real dog profile would look like
const PLACEHOLDER_DOG = {
  id: "placeholder",
  name: "Buddy",
  breed_primary: "Labrador Retriever",
  breed_secondary: "Pit Bull Terrier",
  breed_mixed: true,
  size: "large" as const,
  age_category: "adult" as const,
  age_months: 48,
  gender: "male" as const,
  weight_lbs: 65,
  intake_date: "2023-06-15",
  euthanasia_date: null as string | null,
  urgency_level: "normal" as const,
  description:
    "Buddy is a sweet, energetic boy who loves playing fetch and going for long walks. He's great with kids and other dogs, and would thrive in an active household. He's been waiting for his forever home for a long time and deserves a loving family.",
  good_with_kids: true,
  good_with_dogs: true,
  good_with_cats: false,
  house_trained: true,
  has_special_needs: false,
  has_medical_conditions: false,
  is_heartworm_positive: false,
  is_spayed_neutered: true,
  energy_level: "high" as const,
  adoption_fee: 150,
  is_fee_waived: false,
  shelter_name: "City Animal Shelter",
  shelter_city: "Austin",
  shelter_state: "TX",
  shelter_phone: "(555) 123-4567",
  shelter_email: "adopt@cityshelter.example.com",
  shelter_website: "https://cityshelter.example.com",
  shelter_id: "shelter-1",
};

export default async function DogProfilePage({
  params,
}: {
  params: Promise<{ id: string }>;
}) {
  const { id } = await params;
  const dog = PLACEHOLDER_DOG;

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
            <Link href="/dogs" className="hover:text-gray-700">
              Dogs
            </Link>
          </li>
          <li>/</li>
          <li className="text-gray-900 font-medium">{dog.name}</li>
        </ol>
      </nav>

      {/* Placeholder Notice */}
      <div className="mb-6 p-4 bg-amber-50 border border-amber-200 rounded-lg text-sm text-amber-800">
        This is a placeholder profile. Connect Supabase to load real dog data.
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
        {/* Left Column: Photo + Gallery */}
        <div className="lg:col-span-2 space-y-6">
          {/* Main Photo */}
          <div className="aspect-[4/3] bg-gray-200 rounded-xl flex items-center justify-center overflow-hidden">
            <div className="text-center text-gray-400">
              <svg
                className="mx-auto w-24 h-24"
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
              <p className="mt-2 text-sm">Photo will load from database</p>
            </div>
          </div>

          {/* Thumbnail Gallery Placeholder */}
          <div className="flex gap-2 overflow-x-auto pb-2">
            {Array.from({ length: 4 }).map((_, i) => (
              <div
                key={i}
                className="w-20 h-20 flex-shrink-0 bg-gray-200 rounded-lg"
              />
            ))}
          </div>

          {/* Wait Time Counter */}
          <div className="bg-gray-900 rounded-xl p-6 text-center">
            <p className="text-gray-400 text-sm mb-3">
              {dog.name} has been waiting for a home for
            </p>
            <LEDCounter intakeDate={dog.intake_date} />
            <p className="text-gray-500 text-xs mt-3">
              Since {new Date(dog.intake_date).toLocaleDateString("en-US", {
                year: "numeric",
                month: "long",
                day: "numeric",
              })}
            </p>
          </div>

          {/* Euthanasia Countdown (conditional) */}
          {dog.euthanasia_date && (
            <div className="bg-red-900 rounded-xl p-6 text-center border-2 border-red-500">
              <p className="text-red-300 text-sm mb-3 font-semibold">
                TIME REMAINING FOR {dog.name.toUpperCase()}
              </p>
              <CountdownTimer euthanasiaDate={dog.euthanasia_date} />
              <p className="text-red-400 text-xs mt-3">
                Scheduled euthanasia date:{" "}
                {new Date(dog.euthanasia_date).toLocaleDateString()}
              </p>
            </div>
          )}

          {/* Description */}
          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <h2 className="text-xl font-bold text-gray-900 mb-3">
              About {dog.name}
            </h2>
            <p className="text-gray-700 leading-relaxed">{dog.description}</p>
          </div>

          {/* Details Grid */}
          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <h2 className="text-xl font-bold text-gray-900 mb-4">Details</h2>
            <div className="grid grid-cols-2 sm:grid-cols-3 gap-4">
              <DetailItem label="Breed" value={`${dog.breed_primary}${dog.breed_secondary ? ` / ${dog.breed_secondary}` : ""}${dog.breed_mixed ? " (Mix)" : ""}`} />
              <DetailItem label="Age" value={dog.age_months ? `${Math.floor(dog.age_months / 12)} years ${dog.age_months % 12} months` : dog.age_category} />
              <DetailItem label="Size" value={dog.size.charAt(0).toUpperCase() + dog.size.slice(1)} />
              <DetailItem label="Gender" value={dog.gender.charAt(0).toUpperCase() + dog.gender.slice(1)} />
              <DetailItem label="Weight" value={dog.weight_lbs ? `${dog.weight_lbs} lbs` : "Unknown"} />
              <DetailItem label="Energy Level" value={dog.energy_level ? dog.energy_level.charAt(0).toUpperCase() + dog.energy_level.slice(1) : "Unknown"} />
              <DetailItem label="Spayed/Neutered" value={dog.is_spayed_neutered ? "Yes" : "No"} />
              <DetailItem label="House Trained" value={dog.house_trained ? "Yes" : dog.house_trained === false ? "No" : "Unknown"} />
              <DetailItem
                label="Adoption Fee"
                value={
                  dog.is_fee_waived
                    ? "Fee Waived!"
                    : dog.adoption_fee
                      ? `$${dog.adoption_fee}`
                      : "Contact Shelter"
                }
              />
            </div>
          </div>

          {/* Compatibility */}
          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <h2 className="text-xl font-bold text-gray-900 mb-4">
              Compatibility
            </h2>
            <div className="grid grid-cols-1 sm:grid-cols-3 gap-4">
              <CompatibilityItem
                label="Good with Kids"
                value={dog.good_with_kids}
              />
              <CompatibilityItem
                label="Good with Dogs"
                value={dog.good_with_dogs}
              />
              <CompatibilityItem
                label="Good with Cats"
                value={dog.good_with_cats}
              />
            </div>
          </div>

          {/* Medical Info */}
          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <h2 className="text-xl font-bold text-gray-900 mb-4">
              Medical Information
            </h2>
            <div className="space-y-2">
              <MedicalItem
                label="Special Needs"
                value={dog.has_special_needs}
              />
              <MedicalItem
                label="Medical Conditions"
                value={dog.has_medical_conditions}
              />
              <MedicalItem
                label="Heartworm Positive"
                value={dog.is_heartworm_positive}
              />
            </div>
          </div>
        </div>

        {/* Right Column: CTA + Shelter Info + Share */}
        <div className="space-y-6">
          {/* Dog Name + Badge */}
          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <div className="flex items-start justify-between mb-2">
              <h1 className="text-2xl font-bold text-gray-900">{dog.name}</h1>
              <UrgencyBadge level={dog.urgency_level} />
            </div>
            <p className="text-gray-600 text-sm mb-1">
              {dog.breed_primary}
              {dog.breed_secondary ? ` / ${dog.breed_secondary}` : ""}
            </p>
            <p className="text-gray-500 text-sm">
              {dog.shelter_city}, {dog.shelter_state}
            </p>
          </div>

          {/* CTA Button */}
          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <button className="w-full py-4 px-6 bg-green-500 hover:bg-green-600 text-white text-lg font-bold rounded-lg transition shadow-lg shadow-green-500/25">
              I&apos;m Interested in {dog.name}!
            </button>
            <p className="text-xs text-gray-500 text-center mt-3">
              Clicking this will connect you with the shelter
            </p>
          </div>

          {/* Shelter Info */}
          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <h3 className="font-bold text-gray-900 mb-4">Shelter Information</h3>
            <div className="space-y-3">
              <div>
                <p className="text-sm text-gray-500">Shelter</p>
                <Link
                  href={`/shelters/${dog.shelter_id}`}
                  className="text-blue-600 hover:underline font-medium"
                >
                  {dog.shelter_name}
                </Link>
              </div>
              <div>
                <p className="text-sm text-gray-500">Location</p>
                <p className="font-medium text-gray-900">
                  {dog.shelter_city}, {dog.shelter_state}
                </p>
              </div>
              <div>
                <p className="text-sm text-gray-500">Phone</p>
                <a
                  href={`tel:${dog.shelter_phone}`}
                  className="text-blue-600 hover:underline font-medium"
                >
                  {dog.shelter_phone}
                </a>
              </div>
              <div>
                <p className="text-sm text-gray-500">Email</p>
                <a
                  href={`mailto:${dog.shelter_email}`}
                  className="text-blue-600 hover:underline font-medium text-sm break-all"
                >
                  {dog.shelter_email}
                </a>
              </div>
              <div>
                <p className="text-sm text-gray-500">Website</p>
                <a
                  href={dog.shelter_website}
                  target="_blank"
                  rel="noopener noreferrer"
                  className="text-blue-600 hover:underline font-medium text-sm break-all"
                >
                  {dog.shelter_website}
                </a>
              </div>
            </div>
          </div>

          {/* Share Buttons */}
          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <h3 className="font-bold text-gray-900 mb-4">
              Share {dog.name}&apos;s Profile
            </h3>
            <p className="text-sm text-gray-500 mb-4">
              Sharing saves lives. Help {dog.name} find a home.
            </p>
            <div className="grid grid-cols-2 gap-2">
              <button className="flex items-center justify-center gap-2 px-4 py-2.5 bg-blue-600 hover:bg-blue-700 text-white text-sm font-medium rounded-lg transition">
                <svg className="w-4 h-4" fill="currentColor" viewBox="0 0 24 24">
                  <path d="M24 12.073c0-6.627-5.373-12-12-12s-12 5.373-12 12c0 5.99 4.388 10.954 10.125 11.854v-8.385H7.078v-3.47h3.047V9.43c0-3.007 1.792-4.669 4.533-4.669 1.312 0 2.686.235 2.686.235v2.953H15.83c-1.491 0-1.956.925-1.956 1.874v2.25h3.328l-.532 3.47h-2.796v8.385C19.612 23.027 24 18.062 24 12.073z" />
                </svg>
                Facebook
              </button>
              <button className="flex items-center justify-center gap-2 px-4 py-2.5 bg-black hover:bg-gray-800 text-white text-sm font-medium rounded-lg transition">
                <svg className="w-4 h-4" fill="currentColor" viewBox="0 0 24 24">
                  <path d="M18.244 2.25h3.308l-7.227 8.26 8.502 11.24H16.17l-5.214-6.817L4.99 21.75H1.68l7.73-8.835L1.254 2.25H8.08l4.713 6.231zm-1.161 17.52h1.833L7.084 4.126H5.117z" />
                </svg>
                X / Twitter
              </button>
              <button className="flex items-center justify-center gap-2 px-4 py-2.5 bg-green-600 hover:bg-green-700 text-white text-sm font-medium rounded-lg transition">
                <svg className="w-4 h-4" fill="currentColor" viewBox="0 0 24 24">
                  <path d="M.057 24l1.687-6.163c-1.041-1.804-1.588-3.849-1.587-5.946.003-6.556 5.338-11.891 11.893-11.891 3.181.001 6.167 1.24 8.413 3.488 2.245 2.248 3.481 5.236 3.48 8.414-.003 6.557-5.338 11.892-11.893 11.892-1.99-.001-3.951-.5-5.688-1.448l-6.305 1.654zm6.597-3.807c1.676.995 3.276 1.591 5.392 1.592 5.448 0 9.886-4.434 9.889-9.885.002-5.462-4.415-9.89-9.881-9.892-5.452 0-9.887 4.434-9.889 9.884-.001 2.225.651 3.891 1.746 5.634l-.999 3.648 3.742-.981z" />
                </svg>
                WhatsApp
              </button>
              <button className="flex items-center justify-center gap-2 px-4 py-2.5 bg-gray-200 hover:bg-gray-300 text-gray-700 text-sm font-medium rounded-lg transition">
                <svg
                  className="w-4 h-4"
                  fill="none"
                  viewBox="0 0 24 24"
                  stroke="currentColor"
                >
                  <path
                    strokeLinecap="round"
                    strokeLinejoin="round"
                    strokeWidth={2}
                    d="M8 5H6a2 2 0 00-2 2v12a2 2 0 002 2h10a2 2 0 002-2v-1M8 5a2 2 0 002 2h2a2 2 0 002-2M8 5a2 2 0 012-2h2a2 2 0 012 2m0 0h2a2 2 0 012 2v3m2 4H10m0 0l3-3m-3 3l3 3"
                  />
                </svg>
                Copy Link
              </button>
            </div>
          </div>

          {/* Foster CTA */}
          <div className="bg-green-50 rounded-xl p-6 border border-green-200">
            <h3 className="font-bold text-green-900 mb-2">
              Can&apos;t Adopt? Foster!
            </h3>
            <p className="text-sm text-green-700 mb-4">
              Foster homes have 14x better adoption outcomes. Even temporary
              fostering saves lives.
            </p>
            <Link
              href="/foster"
              className="inline-block w-full text-center py-2.5 px-4 bg-green-600 hover:bg-green-700 text-white font-medium rounded-lg transition text-sm"
            >
              Learn About Fostering
            </Link>
          </div>
        </div>
      </div>
    </div>
  );
}

function DetailItem({ label, value }: { label: string; value: string }) {
  return (
    <div>
      <p className="text-xs text-gray-500 uppercase tracking-wide">{label}</p>
      <p className="font-medium text-gray-900 mt-0.5">{value}</p>
    </div>
  );
}

function CompatibilityItem({
  label,
  value,
}: {
  label: string;
  value: boolean | null;
}) {
  const display =
    value === true
      ? { text: "Yes", color: "text-green-600", bg: "bg-green-50" }
      : value === false
        ? { text: "No", color: "text-red-600", bg: "bg-red-50" }
        : { text: "Unknown", color: "text-gray-500", bg: "bg-gray-50" };

  return (
    <div className={`rounded-lg p-3 text-center ${display.bg}`}>
      <p className="text-xs text-gray-500 mb-1">{label}</p>
      <p className={`font-semibold ${display.color}`}>{display.text}</p>
    </div>
  );
}

function MedicalItem({ label, value }: { label: string; value: boolean }) {
  return (
    <div className="flex items-center justify-between py-2 border-b border-gray-100 last:border-0">
      <span className="text-gray-700 text-sm">{label}</span>
      <span
        className={`text-sm font-medium ${
          value ? "text-amber-600" : "text-green-600"
        }`}
      >
        {value ? "Yes" : "No"}
      </span>
    </div>
  );
}
