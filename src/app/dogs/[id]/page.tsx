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
import PhotoLightbox from "@/components/ui/PhotoLightbox";
import ProductRecommendations from "@/components/monetization/ProductRecommendations";
import type { UrgencyLevel } from "@/lib/constants";
import { generateDogJsonLd, generateBreadcrumbJsonLd } from "@/lib/utils/json-ld";

export async function generateMetadata({
  params,
}: {
  params: Promise<{ id: string }>;
}): Promise<Metadata> {
  try {
    const { id } = await params;
    const supabase = await createClient();
    const { data: dog } = await supabase
      .from("dogs")
      .select("name, breed_primary, primary_photo_url")
      .eq("id", id)
      .single();

    const name = dog?.name || "Dog Profile";
    const breed = dog?.breed_primary || "Adoptable Dog";
    const dogUrl = `https://waitingthelongest.com/dogs/${id}`;
    return {
      title: `${name} - ${breed}`,
      description: `Meet ${name}, a ${breed} waiting for a forever home. View their full profile and find out how to adopt.`,
      openGraph: {
        title: `${name} | WaitingTheLongest.com`,
        description: `${name} is waiting for a loving home. See their full profile and find out how to help.`,
        images: dog?.primary_photo_url ? [dog.primary_photo_url] : undefined,
        url: dogUrl,
      },
      twitter: {
        card: "summary_large_image",
        title: `${name} - ${breed} | WaitingTheLongest.com`,
        description: `Meet ${name}, a ${breed} waiting for a forever home. Help save a life today.`,
        images: dog?.primary_photo_url ? [dog.primary_photo_url] : undefined,
      },
    };
  } catch {
    return { title: "Dog Profile" };
  }
}

export default async function DogProfilePage({
  params,
}: {
  params: Promise<{ id: string }>;
}) {
  const { id } = await params;

  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  let dog: any = null;
  try {
    const supabase = await createClient();

    const { data, error } = await supabase
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

    if (error || !data) {
      notFound();
    }
    dog = data;

    // Fetch similar dogs (same primary breed, still available)
    const { data: similarDogsData } = await supabase
      .from("dogs")
      .select("id, name, breed_primary, primary_photo_url, intake_date, age_category, size, gender, shelters!inner(name, city, state_code)")
      .eq("is_available", true)
      .neq("id", data.id)
      .eq("breed_primary", data.breed_primary)
      .limit(4);

    dog._similarDogs = similarDogsData || [];
  } catch {
    notFound();
  }

  if (!dog) {
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
  const isUrgent = urgency === "critical" || urgency === "high";
  const photos: string[] = dog.photo_urls || [];
  const mainPhoto = dog.primary_photo_url || photos[0] || null;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const similarDogs: any[] = dog._similarDogs || [];

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      <script
        type="application/ld+json"
        dangerouslySetInnerHTML={{
          __html: JSON.stringify(generateDogJsonLd(dog, shelter)),
        }}
      />
      <script
        type="application/ld+json"
        dangerouslySetInnerHTML={{
          __html: JSON.stringify(generateBreadcrumbJsonLd([
            { name: "Home", url: "https://waitingthelongest.com" },
            { name: "Dogs", url: "https://waitingthelongest.com/dogs" },
            { name: dog.name, url: `https://waitingthelongest.com/dogs/${dog.id}` },
          ])),
        }}
      />
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
          <div className="aspect-[4/3] bg-gray-100 rounded-xl overflow-hidden relative group">
            {mainPhoto ? (
              <a href={mainPhoto} target="_blank" rel="noopener noreferrer" title="Click to enlarge">
                <Image
                  src={mainPhoto}
                  alt={`${dog.name} - ${dog.breed_primary}`}
                  fill
                  className="object-cover"
                  sizes="(max-width: 1024px) 100vw, 66vw"
                  priority
                />
                <div className="absolute inset-0 bg-black/0 group-hover:bg-black/10 transition flex items-end justify-center pb-4 opacity-0 group-hover:opacity-100">
                  <span className="bg-black/60 text-white text-xs px-3 py-1.5 rounded-full">Click to enlarge</span>
                </div>
              </a>
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
            <PhotoLightbox photos={photos} dogName={dog.name} />
          )}

          {/* Wait Time Counter */}
          <div className="bg-gray-900 rounded-xl p-6 text-center">
            <p className="text-gray-400 text-sm mb-3">{dog.name} has been waiting for a home for</p>
            <LEDCounter intakeDate={dog.intake_date} />
            <p className="text-gray-500 text-xs mt-3">
              Since {new Date(dog.intake_date).toLocaleDateString("en-US", { year: "numeric", month: "long", day: "numeric" })}
            </p>
            <DateAccuracyBadge confidence={dog.date_confidence} source={dog.date_source} />
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
              <DetailItem label="Age (Human Years)" value={humanYears(dog.age_months, dog.age_category)} />
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

          {/* Similar Dogs */}
          {similarDogs && similarDogs.length > 0 && (
            <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
              <h2 className="text-xl font-bold text-gray-900 mb-4">Similar Dogs Available</h2>
              <div className="grid grid-cols-2 sm:grid-cols-4 gap-3">
                {similarDogs.map((sd: any) => (
                  <Link key={sd.id} href={`/dogs/${sd.id}`} className="group">
                    <div className="aspect-square bg-gray-100 rounded-lg overflow-hidden relative">
                      {sd.primary_photo_url ? (
                        <Image src={sd.primary_photo_url} alt={sd.name} fill className="object-cover group-hover:scale-105 transition" sizes="150px" />
                      ) : (
                        <div className="w-full h-full flex items-center justify-center bg-gray-200">
                          <span className="text-gray-400 text-2xl">&#x1F415;</span>
                        </div>
                      )}
                    </div>
                    <p className="text-sm font-medium text-gray-900 mt-1 truncate group-hover:text-blue-600">{sd.name}</p>
                    <p className="text-xs text-gray-500 truncate">{sd.breed_primary}</p>
                  </Link>
                ))}
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
            {dog.view_count > 0 && (
              <p className="text-xs text-gray-400 mt-1">{dog.view_count.toLocaleString()} people have viewed this profile</p>
            )}

            {/* Verification Status */}
            <div className="mt-3 pt-3 border-t border-gray-100">
              <VerificationBadge
                status={dog.verification_status || "unverified"}
                lastVerified={dog.last_verified_at}
                intakeDate={dog.intake_date}
              />
            </div>
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
              {isUrgent && shelter.phone && (
                <a href={`tel:${shelter.phone}`} className="block w-full text-center py-3 px-4 bg-red-600 hover:bg-red-700 text-white font-bold rounded-lg transition text-sm mt-3">
                  Call Shelter NOW - {shelter.phone}
                </a>
              )}
            </div>
          )}

          <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
            <h3 className="font-bold text-gray-900 mb-2">Share {dog.name}&apos;s Profile</h3>
            <p className="text-sm text-gray-500 mb-4">Sharing saves lives. Help {dog.name} find a home.</p>
            <ShareButtons dogName={dog.name} isUrgent={urgency === "critical" || urgency === "high"} euthanasiaDate={dog.euthanasia_date} />
          </div>

          <div className="bg-green-50 rounded-xl p-6 border border-green-200">
            <h3 className="font-bold text-green-900 mb-2">Can&apos;t Adopt? Foster!</h3>
            <p className="text-sm text-green-700 mb-4">Foster homes have 14x better adoption outcomes.</p>
            <Link href="/foster" className="inline-block w-full text-center py-2.5 px-4 bg-green-600 hover:bg-green-700 text-white font-medium rounded-lg transition text-sm">
              Learn About Fostering
            </Link>
          </div>

          {/* Product Recommendations (hidden for urgent dogs) */}
          <ProductRecommendations
            dogId={dog.id}
            dogName={dog.name}
            breed={dog.breed_primary}
            size={dog.size}
            ageText={dog.age_category}
            urgencyLevel={urgency}
          />

          {/* Data Accuracy Disclaimer */}
          <div className="bg-gray-50 rounded-xl p-4 border border-gray-200">
            <p className="text-[11px] text-gray-500 leading-relaxed">
              <strong className="text-gray-600">Important:</strong> Listing data comes from RescueGroups.org and is maintained by the shelter or rescue.
              We cannot guarantee that any dog is still available for adoption. Always contact the shelter directly
              to confirm availability before visiting.
              {dog.external_url && (
                <> View the <a href={dog.external_url} target="_blank" rel="noopener noreferrer" className="text-blue-500 hover:underline">original listing</a>.</>
              )}
            </p>
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

function humanYears(ageMonths: number | null, ageCategory: string): string {
  if (!ageMonths) {
    // Estimate from category if no age_months
    const categoryMap: Record<string, number> = { puppy: 6, young: 24, adult: 60, senior: 108 };
    const estimated = categoryMap[ageCategory?.toLowerCase()] || null;
    if (!estimated) return "Unknown";
    ageMonths = estimated;
  }
  let hy: number;
  if (ageMonths <= 12) {
    // Puppy (0-12 months)
    hy = ageMonths * 2;
  } else if (ageMonths <= 36) {
    // Young (1-3 years)
    hy = 15 + (ageMonths - 12) * 0.75;
  } else if (ageMonths <= 84) {
    // Adult (3-7 years)
    hy = 28 + (ageMonths - 36) * 0.5;
  } else {
    // Senior (7+ years)
    hy = 56 + (ageMonths - 84) * 0.4;
  }
  return `~${Math.round(hy)} human years`;
}

function DateAccuracyBadge({ confidence, source }: { confidence: string | null; source: string | null }) {
  if (!confidence) return null;

  const isVerified = confidence === "verified";
  const isHigh = confidence === "high";
  const isLow = confidence === "low" || confidence === "unknown";

  // Derive a human-readable source label
  let sourceLabel = "";
  if (source?.includes("available_date")) sourceLabel = "Shelter-provided available date";
  else if (source?.includes("found_date")) sourceLabel = "Shelter-recorded found date";
  else if (source?.includes("returned_after_adoption")) sourceLabel = "Returned after adoption";
  else if (source?.includes("description_parsed")) sourceLabel = "Parsed from listing description";
  else if (source?.includes("created_date")) sourceLabel = "Listing creation date";
  else if (source?.includes("updated_date")) sourceLabel = "Listing last updated";
  else if (source?.includes("age_capped")) sourceLabel = "Capped to estimated birth date";
  else if (source?.includes("audit_repair")) sourceLabel = "Estimated from available data";

  if (isVerified) {
    return (
      <div className="mt-3 flex items-center justify-center gap-1.5">
        <span className="inline-flex items-center gap-1 px-2 py-0.5 text-[10px] bg-green-900/50 text-green-400 rounded-full border border-green-700/50">
          <svg className="w-2.5 h-2.5" fill="currentColor" viewBox="0 0 20 20">
            <path fillRule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clipRule="evenodd" />
          </svg>
          Verified wait time
        </span>
        {sourceLabel && <span className="text-[10px] text-gray-600">{sourceLabel}</span>}
      </div>
    );
  }

  if (isHigh) {
    return (
      <div className="mt-2">
        <span className="text-[10px] text-gray-500">Wait time based on {sourceLabel || "available listing data"}</span>
      </div>
    );
  }

  return (
    <div className="mt-3 flex items-center justify-center gap-1.5">
      <span className={`inline-flex items-center gap-1 px-2 py-0.5 text-[10px] rounded-full border ${isLow ? "bg-amber-900/30 text-amber-400 border-amber-700/50" : "bg-gray-800 text-gray-400 border-gray-700"}`}>
        <svg className="w-2.5 h-2.5" fill="currentColor" viewBox="0 0 20 20">
          <path fillRule="evenodd" d="M18 10a8 8 0 11-16 0 8 8 0 0116 0zm-7 4a1 1 0 11-2 0 1 1 0 012 0zm-1-9a1 1 0 00-1 1v4a1 1 0 102 0V6a1 1 0 00-1-1z" clipRule="evenodd" />
        </svg>
        Estimated wait time
      </span>
      {sourceLabel && <span className="text-[10px] text-gray-600">{sourceLabel}</span>}
    </div>
  );
}

function VerificationBadge({ status, lastVerified, intakeDate }: { status: string; lastVerified: string | null; intakeDate: string | null }) {
  const verifiedAgo = lastVerified
    ? Math.floor((Date.now() - new Date(lastVerified).getTime()) / (1000 * 60 * 60 * 1000))
    : null;
  const verifiedAgoDays = verifiedAgo !== null ? Math.floor(verifiedAgo / 24) : null;

  // Flag listings older than 2 years as suspicious
  const waitDays = intakeDate
    ? Math.floor((Date.now() - new Date(intakeDate).getTime()) / (1000 * 60 * 60 * 24))
    : 0;
  const isSuspiciouslyOld = waitDays > 730; // > 2 years

  switch (status) {
    case "verified":
      return (
        <div>
          <div className="flex items-center gap-1.5">
            <span className="inline-flex items-center gap-1 px-2 py-0.5 text-xs bg-blue-50 text-blue-700 rounded-full font-medium border border-blue-200">
              <svg className="w-3 h-3" fill="currentColor" viewBox="0 0 20 20">
                <path fillRule="evenodd" d="M16.707 5.293a1 1 0 010 1.414l-8 8a1 1 0 01-1.414 0l-4-4a1 1 0 011.414-1.414L8 12.586l7.293-7.293a1 1 0 011.414 0z" clipRule="evenodd" />
              </svg>
              Listing Active
            </span>
            {verifiedAgoDays !== null && (
              <span className="text-xs text-gray-400">
                checked {verifiedAgoDays === 0 ? "today" : `${verifiedAgoDays}d ago`}
              </span>
            )}
          </div>
          <p className="text-[10px] text-gray-400 mt-1">
            Source listing exists on RescueGroups.org. Contact the shelter directly to confirm availability.
          </p>
          {isSuspiciouslyOld && (
            <p className="text-[10px] text-amber-600 mt-1 font-medium">
              This listing is {Math.floor(waitDays / 365)}+ years old. We recommend calling the shelter to confirm this dog is still available.
            </p>
          )}
        </div>
      );
    case "not_found":
      return (
        <div>
          <span className="inline-flex items-center gap-1 px-2 py-0.5 text-xs bg-amber-50 text-amber-700 rounded-full font-medium border border-amber-200">
            <svg className="w-3 h-3" fill="currentColor" viewBox="0 0 20 20">
              <path fillRule="evenodd" d="M8.257 3.099c.765-1.36 2.722-1.36 3.486 0l5.58 9.92c.75 1.334-.213 2.98-1.742 2.98H4.42c-1.53 0-2.493-1.646-1.743-2.98l5.58-9.92zM11 13a1 1 0 11-2 0 1 1 0 012 0zm-1-8a1 1 0 00-1 1v3a1 1 0 002 0V6a1 1 0 00-1-1z" clipRule="evenodd" />
            </svg>
            May No Longer Be Available
          </span>
          <p className="text-[10px] text-amber-600 mt-1">
            This listing was not found on RescueGroups.org. The dog may have been adopted or the listing removed.
          </p>
        </div>
      );
    case "pending":
      return (
        <span className="inline-flex items-center gap-1 px-2 py-0.5 text-xs bg-blue-50 text-blue-700 rounded-full font-medium border border-blue-200">
          Adoption Pending
        </span>
      );
    default:
      return (
        <div>
          <span className="inline-flex items-center gap-1 px-2 py-0.5 text-xs bg-gray-50 text-gray-500 rounded-full font-medium border border-gray-200">
            Not Yet Verified
          </span>
          <p className="text-[10px] text-gray-400 mt-1">
            This listing has not been checked against the source. Contact the shelter to confirm availability.
          </p>
          {isSuspiciouslyOld && (
            <p className="text-[10px] text-amber-600 mt-1 font-medium">
              This listing is {Math.floor(waitDays / 365)}+ years old. We recommend calling the shelter to confirm this dog is still available.
            </p>
          )}
        </div>
      );
  }
}
