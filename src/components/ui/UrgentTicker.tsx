"use client";

import { useEffect, useState } from "react";
import Link from "next/link";

interface TickerDog {
  id: string;
  name: string;
  breed_primary: string;
  shelter_city?: string;
  shelter_state?: string;
  euthanasia_date: string;
}

export default function UrgentTicker() {
  const [dogs, setDogs] = useState<TickerDog[]>([]);

  useEffect(() => {
    fetch("/api/dogs?urgency=critical&sort=urgency&limit=10")
      .then(res => res.json())
      .then(data => {
        if (data.dogs) {
          setDogs(data.dogs.map((d: any) => ({
            id: d.id,
            name: d.name,
            breed_primary: d.breed_primary,
            shelter_city: d.shelters?.city || d.shelter_city,
            shelter_state: d.shelters?.state_code || d.shelter_state,
            euthanasia_date: d.euthanasia_date,
          })));
        }
      })
      .catch(() => {});
  }, []);

  if (dogs.length === 0) return null;

  function getTimeLeft(date: string) {
    const diff = new Date(date).getTime() - Date.now();
    if (diff <= 0) return "TIME UP";
    const hours = Math.floor(diff / 3600000);
    const mins = Math.floor((diff % 3600000) / 60000);
    if (hours > 24) return `${Math.floor(hours / 24)}d ${hours % 24}h`;
    return `${hours}h ${mins}m`;
  }

  return (
    <div className="bg-red-900 text-white overflow-hidden relative" role="alert" aria-label="Urgent dogs needing rescue">
      <div className="flex items-center">
        <div className="bg-red-700 px-3 py-1.5 text-xs font-bold uppercase tracking-wider flex-shrink-0 flex items-center gap-1.5 z-10">
          <span className="w-2 h-2 bg-red-400 rounded-full animate-pulse" />
          URGENT
        </div>
        <div className="overflow-hidden flex-1">
          <div className="flex animate-ticker whitespace-nowrap py-1.5">
            {[...dogs, ...dogs].map((dog, i) => (
              <Link
                key={`${dog.id}-${i}`}
                href={`/dogs/${dog.id}`}
                className="inline-flex items-center gap-2 px-4 text-sm hover:text-red-200 transition flex-shrink-0"
              >
                <span className="font-semibold">{dog.name}</span>
                <span className="text-red-300 text-xs">
                  {dog.breed_primary} - {[dog.shelter_city, dog.shelter_state].filter(Boolean).join(", ")}
                </span>
                <span className="text-red-400 font-mono text-xs font-bold bg-red-950 px-1.5 py-0.5 rounded">
                  {getTimeLeft(dog.euthanasia_date)}
                </span>
              </Link>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}
