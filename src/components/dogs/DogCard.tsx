"use client";

import Link from "next/link";
import Image from "next/image";
import LEDCounter from "@/components/counters/LEDCounter";
import CountdownTimer from "@/components/counters/CountdownTimer";
import UrgencyBadge from "@/components/ui/UrgencyBadge";
import SaveButton from "@/components/ui/SaveButton";
import type { Dog } from "@/types/dog";
import type { UrgencyLevel } from "@/lib/constants";

interface DogCardProps {
  dog: Dog & { shelters?: { name: string; city: string; state_code: string } };
  showCountdown?: boolean;
  urgencyHighlight?: boolean;
}

export default function DogCard({
  dog,
  showCountdown = false,
  urgencyHighlight = false,
}: DogCardProps) {
  const shelterName =
    dog.shelter_name || dog.shelters?.name || "Unknown Shelter";
  const shelterCity = dog.shelter_city || dog.shelters?.city || "";
  const shelterState = dog.shelter_state || dog.shelters?.state_code || "";
  const location = [shelterCity, shelterState].filter(Boolean).join(", ");

  const urgency = (dog.urgency_level || "normal") as UrgencyLevel;
  const isUrgent = urgency === "critical" || urgency === "high";
  const borderClass = urgencyHighlight
    ? urgency === "critical"
      ? "border-red-400 ring-2 ring-red-300"
      : urgency === "high"
        ? "border-orange-400 ring-1 ring-orange-200"
        : urgency === "medium"
          ? "border-yellow-300"
          : "border-gray-200"
    : "border-gray-200";

  return (
    <Link
      href={`/dogs/${dog.id}`}
      className={`group bg-white rounded-lg shadow-sm border ${borderClass} overflow-hidden hover:shadow-md transition-all block focus:outline-none focus:ring-2 focus:ring-blue-400 focus:ring-offset-2`}
    >
      {/* Photo */}
      <div className="aspect-square bg-gray-100 relative overflow-hidden">
        {dog.primary_photo_url ? (
          <Image
            src={dog.primary_photo_url}
            alt={`${dog.name} - ${dog.breed_primary}`}
            fill
            className="object-cover group-hover:scale-105 transition-transform duration-300"
            sizes="(max-width: 640px) 100vw, (max-width: 1024px) 50vw, 33vw"
          />
        ) : (
          <div className="w-full h-full flex items-center justify-center bg-gray-200">
            <svg
              className="w-16 h-16 text-gray-400"
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
        )}
        {/* Urgency badge overlay */}
        {isUrgent && (
          <div className="absolute top-2 right-2">
            <UrgencyBadge level={urgency} />
          </div>
        )}
        {/* Fee waived badge */}
        {dog.is_fee_waived && (
          <div className="absolute top-2 left-2 bg-green-500 text-white text-xs font-bold px-2 py-0.5 rounded">
            FEE WAIVED
          </div>
        )}
        {/* Save button */}
        <div className="absolute bottom-2 right-2">
          <SaveButton dogId={dog.id} dogName={dog.name} breed={dog.breed_primary} photo={dog.primary_photo_url} />
        </div>
      </div>

      {/* Card Content */}
      <div className="p-4">
        <div className="flex items-start justify-between mb-1">
          <h3 className="font-bold text-gray-900 group-hover:text-blue-600 transition-colors truncate">
            {dog.name}
          </h3>
          {!isUrgent && <UrgencyBadge level={urgency} className="text-[10px] ml-1 flex-shrink-0" />}
        </div>
        <p className="text-sm text-gray-600 truncate">
          {dog.breed_primary}
          {dog.breed_secondary ? ` / ${dog.breed_secondary}` : ""}
          {dog.breed_mixed ? " Mix" : ""}
        </p>
        <p className="text-xs text-gray-500 mt-0.5">
          {dog.size && dog.size.charAt(0).toUpperCase() + dog.size.slice(1)}{" "}
          &middot;{" "}
          {dog.age_category &&
            dog.age_category.charAt(0).toUpperCase() + dog.age_category.slice(1)}{" "}
          &middot;{" "}
          {dog.gender &&
            dog.gender.charAt(0).toUpperCase() + dog.gender.slice(1)}
        </p>
        {location && (
          <p className="text-xs text-gray-400 mt-1">{location}</p>
        )}

        {/* Countdown timer for urgent dogs */}
        {showCountdown && dog.euthanasia_date && (
          <div className="mt-3">
            <CountdownTimer
              euthanasiaDate={dog.euthanasia_date}
              compact
            />
          </div>
        )}

        {/* LED Wait Time Counter — always show if dog has intake_date and no countdown shown */}
        {dog.intake_date && !(showCountdown && dog.euthanasia_date) && (
          <div className="mt-3">
            <LEDCounter intakeDate={dog.intake_date} compact />
            {(dog.date_confidence === "low" || dog.date_confidence === "unknown") && (
              <p className="text-[9px] text-gray-400 mt-0.5 italic">~ estimated wait time</p>
            )}
          </div>
        )}
      </div>
    </Link>
  );
}
