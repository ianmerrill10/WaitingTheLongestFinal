import type { Dog } from "@/types/dog";
import DogCard from "./DogCard";

interface DogGridProps {
  dogs: (Dog & { shelters?: { name: string; city: string; state_code: string } })[];
  showCountdown?: boolean;
  urgencyHighlight?: boolean;
  emptyMessage?: string;
}

export default function DogGrid({
  dogs,
  showCountdown = false,
  urgencyHighlight = false,
  emptyMessage = "No dogs found.",
}: DogGridProps) {
  if (dogs.length === 0) {
    return (
      <div className="text-center py-12 bg-white rounded-lg border border-dashed border-gray-300">
        <svg
          className="mx-auto h-12 w-12 text-gray-400"
          fill="none"
          viewBox="0 0 24 24"
          stroke="currentColor"
        >
          <path
            strokeLinecap="round"
            strokeLinejoin="round"
            strokeWidth={1}
            d="M20 13V6a2 2 0 00-2-2H6a2 2 0 00-2 2v7m16 0v5a2 2 0 01-2 2H6a2 2 0 01-2-2v-5m16 0h-2.586a1 1 0 00-.707.293l-2.414 2.414a1 1 0 01-.707.293h-3.172a1 1 0 01-.707-.293l-2.414-2.414A1 1 0 006.586 13H4"
          />
        </svg>
        <p className="mt-3 text-gray-500">{emptyMessage}</p>
      </div>
    );
  }

  return (
    <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
      {dogs.map((dog) => (
        <DogCard
          key={dog.id}
          dog={dog}
          showCountdown={showCountdown}
          urgencyHighlight={urgencyHighlight}
        />
      ))}
    </div>
  );
}
