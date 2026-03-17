import type { Metadata } from "next";
import Link from "next/link";
import Image from "next/image";
import { notFound } from "next/navigation";
import { createClient } from "@/lib/supabase/server";
import LEDCounter from "@/components/counters/LEDCounter";
import CountdownTimer from "@/components/counters/CountdownTimer";
import UrgencyBadge from "@/components/ui/UrgencyBadge";
import InterestForm from "@/components/ui/InterestForm";
import ShareButtons from "@/components/ui/ShareButtons";
import type { UrgencyLevel } from "@/lib/constants";

export async function generateMetadata({
  params,
}: {
  params: Promise<{ id: string }>;
}): Promise<Metadata> {
  const { id } = await params;
  const supabase = await createClient();
  const { data: dog } = await supabase
    .from("dogs")
    .select("name, breed_primary, primary_photo_url")
    .eq("id", id)
    .single();

  const name = dog?.name || "Dog Profile";
  const breed = dog?.breed_primary || "Adoptable Dog";
  return {
    title: `${name} - ${breed}`,
    description: `Meet ${name}, a ${breed} waiting for a forever home. View their full profile and find out how to adopt.`,
    openGraph: {
      title: `${name} | WaitingTheLongest.com`,
      description: `${name} is waiting for a loving home. See their full profile and find out how to help.`,
      images: dog?.primary_photo_url ? [dog.primary_photo_url] : undefined,
    },
  };
}

export default async function DogProfilePage({
  params,
}: {
  params: Promise<{ id: string }>;
}) {
  const { id } = await params;
  const supabase = await createClient();

  const { data: dog, error } = await supabase
    .from("dogs")
    .select(
      `
      *,
      shelters (
        id,
        name,
        city,
        state_code,
        phone,
        email,
        website
      )
    `
    )
    .eq("id", id)
    .single();

  if (error || !dog) {
    notFound();
  }

  const shelter = dog.shelters as {
    id: string;
    name: string;
    city: string;
    state_code: string;
    phone: string | null;
    email: string | null;
    website: string | null;
  } | null;

  const urgency = (dog.urgency_level || "normal") as UrgencyLevel;
  const photos: string[] = dog.photo_urls || [];
  const mainPhoto = dog.primary_photo_url || photos[0] || null;

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      {/* Breadcrumb */}
      <nav className="mb-6 text-sm text-gray-500" aria-label="Breadcrumb">
        <ol className="flex items-center gap-2">
          <li><Link href="/" className="hover:text-gray-700">Home</Link></li>
          <li>/</li>
          <li><Link href="/dogs" className="hover:text-gray-700">Dogs</Link></li>
          <li>/</li>
          <li className="text-gray-900 font-medium">{dog.name}</li>
        </ol>
      </nav>

      <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
        {/* Left Column */}
        <div className="lg:col-span-2 space-y-6">
          {/* Main Photo */}
          <div className="aspect-[4/3] bg-gray-100 rounded-xl overflow-hidden relative">
            {mainPhoto ? (
              <Image
                src={mainPhoto}
                alt={`${dog.name} - ${dog.breed_primary}`}
                fill
                className="object-cover"
                sizes="(max-width: 1024px) 100vw, 66vw"
                priority
              />
            ) : (
              <div className="w-full h-full flex items-center justify-center bg-gray-200">
                <svg className="w-24 h-24 text-gray-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1}
                    d="M4 16l4.586-4.586a2 2 0 012.828 0L16 16m-2-2l1.586-1.586a2 2 0 012.828 0L20 14m-6-6h.01M6 20h12a2 2 0 002-2V6a2 2 0 00-2-2H6a2 2 0 00-2 2v12a2 2 0 002 2z" />
                </svg>
              </div>
            )}
          </div>

          {/* Photo Gallery */}
          {photos.length > 1 && (
            <div className="flex gap-2 overflow-x-auto pb-2">
              {photos.map((photo, i) => (
                <div key={i} className="w-20 h-20 flex-shrink-0 rounded-lg overflow-hidden relative bg-gray-100">
                  <Image src={photo} alt={`${dog.name} photo ${i + 1}`} fill className="object-cover" sizes="80px" />
                </div>
              ))}
            </div>
          )}

          {/* Wait Time Counter */}
          <div className="bg-gray-900 rounded-xl p-6 text-center">
            <p className="text-gray-400 text-sm mb-3">{dog.name} has been waiting for a home for</p>
            <LEDCounter intakeDate={dog.intake_date} />
            <p className="text-gray-500 text-xs mt-3">
              Since {new Date(dog.intake_date).toLocaleDateString("en-US", { year: "numeric", month: "long", day: "numeric" })}
            </p>
          </div>

          {/* Euthanasia Countdown */}
          {dog.euthanasia_date && (
            <div className="bg-red-900 rounded-xl p-6 text-center border-2 border-red-500">
              <p className="text-red-300 text-sm mb-3 font-semibold">
                TIME REMAINING FOR {dog.name.toUpperCase()}
              </p>
              <CountdownTimer euthanasiaDate={dog.euthanasia_date} />
              <p className="text-red-400 text-xs mt-3">
                Scheduled: {new Date(dog.euthanasia_date).toLocaleDateString("en-US", { year: "numeric", month: "long", day: "numeric" })}
              </p>
            </div>
          )}

          {/* Description */}
          {dog.description && (
            <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
              <h2 className="text-xl font-bold text-gray-900 mb-3">About {dog.name}</h2>
              <p className="text-gray-700 leading-relaxed whitespace-pre-line">{dog.description}</p>
            </div>
          )}

          {/* Details Grid */}
          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <h2 className="text-xl font-bold text-gray-900 mb-4">Details</h2>
            <div className="grid grid-cols-2 sm:grid-cols-3 gap-4">
              <DetailItem label="Breed" value={`${dog.breed_primary}${dog.breed_secondary ? ` / ${dog.breed_secondary}` : ""}${dog.breed_mixed ? " (Mix)" : ""}`} />
              <DetailItem label="Age" value={dog.age_months ? `${Math.floor(dog.age_months / 12)}y ${dog.age_months % 12}m` : cap(dog.age_category)} />
              <DetailItem label="Size" value={cap(dog.size)} />
              <DetailItem label="Gender" value={cap(dog.gender)} />
              <DetailItem label="Weight" value={dog.weight_lbs ? `${dog.weight_lbs} lbs` : "Unknown"} />
              <DetailItem label="Energy" value={dog.energy_level ? cap(dog.energy_level) : "Unknown"} />
              <DetailItem label="Spayed/Neutered" value={dog.is_spayed_neutered ? "Yes" : "No"} />
              <DetailItem label="House Trained" value={dog.house_trained === true ? "Yes" : dog.house_trained === false ? "No" : "Unknown"} />
              <DetailItem label="Adoption Fee" value={dog.is_fee_waived ? "Fee Waived!" : dog.adoption_fee ? `$${dog.adoption_fee}` : "Contact Shelter"} />
            </div>
          </div>

          {/* Compatibility */}
          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <h2 className="text-xl font-bold text-gray-900 mb-4">Compatibility</h2>
            <div className="grid grid-cols-1 sm:grid-cols-3 gap-4">
              <CompatItem label="Good with Kids" value={dog.good_with_kids} />
              <CompatItem label="Good with Dogs" value={dog.good_with_dogs} />
              <CompatItem label="Good with Cats" value={dog.good_with_cats} />
            </div>
          </div>

          {/* Medical Info */}
          {(dog.has_special_needs || dog.has_medical_conditions || dog.is_heartworm_positive) && (
            <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
              <h2 className="text-xl font-bold text-gray-900 mb-4">Medical Information</h2>
              <div className="space-y-2">
                {dog.has_special_needs && <MedItem label="Special Needs" note={dog.special_needs_description} />}
                {dog.has_medical_conditions && <MedItem label="Medical Conditions" />}
                {dog.is_heartworm_positive && <MedItem label="Heartworm Positive" />}
              </div>
            </div>
          )}
        </div>

        {/* Right Column */}
        <div className="space-y-6">
          {/* Dog Name + Badge */}
          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <div className="flex items-start justify-between mb-2">
              <h1 className="text-2xl font-bold text-gray-900">{dog.name}</h1>
              <UrgencyBadge level={urgency} />
            </div>
            <p className="text-gray-600 text-sm mb-1">
              {dog.breed_primary}{dog.breed_secondary ? ` / ${dog.breed_secondary}` : ""}
            </p>
            {shelter && <p className="text-gray-500 text-sm">{shelter.city}, {shelter.state_code}</p>}
          </div>

          <InterestForm dogId={dog.id} dogName={dog.name} shelterId={dog.shelter_id} />

          {/* Shelter Info */}
          {shelter && (
            <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
              <h3 className="font-bold text-gray-900 mb-4">Shelter Information</h3>
              <div className="space-y-3">
                <div>
                  <p className="text-sm text-gray-500">Shelter</p>
                  <Link href={`/shelters/${shelter.id}`} className="text-blue-600 hover:underline font-medium">{shelter.name}</Link>
                </div>
                <div>
                  <p className="text-sm text-gray-500">Location</p>
                  <p className="font-medium text-gray-900">{shelter.city}, {shelter.state_code}</p>
                </div>
                {shelter.phone && (
                  <div>
                    <p className="text-sm text-gray-500">Phone</p>
                    <a href={`tel:${shelter.phone}`} className="text-blue-600 hover:underline font-medium">{shelter.phone}</a>
                  </div>
                )}
                {shelter.email && (
                  <div>
                    <p className="text-sm text-gray-500">Email</p>
                    <a href={`mailto:${shelter.email}`} className="text-blue-600 hover:underline font-medium text-sm break-all">{shelter.email}</a>
                  </div>
                )}
                {shelter.website && (
                  <div>
                    <p className="text-sm text-gray-500">Website</p>
                    <a href={shelter.website} target="_blank" rel="noopener noreferrer" className="text-blue-600 hover:underline font-medium text-sm">Visit Website</a>
                  </div>
                )}
              </div>
            </div>
          )}

          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <h3 className="font-bold text-gray-900 mb-2">Share {dog.name}&apos;s Profile</h3>
            <p className="text-sm text-gray-500 mb-4">Sharing saves lives. Help {dog.name} find a home.</p>
            <ShareButtons dogName={dog.name} />
          </div>

          <div className="bg-green-50 rounded-xl p-6 border border-green-200">
            <h3 className="font-bold text-green-900 mb-2">Can&apos;t Adopt? Foster!</h3>
            <p className="text-sm text-green-700 mb-4">Foster homes have 14x better adoption outcomes.</p>
            <Link href="/foster" className="inline-block w-full text-center py-2.5 px-4 bg-green-600 hover:bg-green-700 text-white font-medium rounded-lg transition text-sm">
              Learn About Fostering
            </Link>
          </div>
        </div>
      </div>
    </div>
  );
}

function cap(s: string | null): string {
  if (!s) return "Unknown";
  return s.charAt(0).toUpperCase() + s.slice(1);
}

function DetailItem({ label, value }: { label: string; value: string }) {
  return (
    <div>
      <p className="text-xs text-gray-500 uppercase tracking-wide">{label}</p>
      <p className="font-medium text-gray-900 mt-0.5">{value}</p>
    </div>
  );
}

function CompatItem({ label, value }: { label: string; value: boolean | null }) {
  const d = value === true
    ? { text: "Yes", color: "text-green-600", bg: "bg-green-50" }
    : value === false
      ? { text: "No", color: "text-red-600", bg: "bg-red-50" }
      : { text: "Unknown", color: "text-gray-500", bg: "bg-gray-50" };
  return (
    <div className={`rounded-lg p-3 text-center ${d.bg}`}>
      <p className="text-xs text-gray-500 mb-1">{label}</p>
      <p className={`font-semibold ${d.color}`}>{d.text}</p>
    </div>
  );
}

function MedItem({ label, note }: { label: string; note?: string | null }) {
  return (
    <div className="flex items-center justify-between py-2 border-b border-gray-100 last:border-0">
      <div>
        <span className="text-gray-700 text-sm">{label}</span>
        {note && <p className="text-xs text-gray-500 mt-0.5">{note}</p>}
      </div>
      <span className="text-sm font-medium text-amber-600">Yes</span>
    </div>
  );
}
