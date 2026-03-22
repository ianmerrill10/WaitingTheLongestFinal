"use client";

import { useSavedDogs } from "@/lib/hooks/useSavedDogs";
import Link from "next/link";
import Image from "next/image";

export default function SavedDogsPage() {
  const { savedDogs, unsaveDog } = useSavedDogs();

  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      <h1 className="text-3xl font-bold text-gray-900 mb-2">Saved Dogs</h1>
      <p className="text-gray-500 mb-8">Dogs you&apos;ve saved are stored in your browser. No account needed.</p>

      {savedDogs.length === 0 ? (
        <div className="text-center py-16">
          <svg className="w-16 h-16 text-gray-300 mx-auto mb-4" fill="none" viewBox="0 0 24 24" stroke="currentColor">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M4.318 6.318a4.5 4.5 0 000 6.364L12 20.364l7.682-7.682a4.5 4.5 0 00-6.364-6.364L12 7.636l-1.318-1.318a4.5 4.5 0 00-6.364 0z" />
          </svg>
          <p className="text-gray-500 text-lg mb-4">No saved dogs yet</p>
          <p className="text-gray-400 text-sm mb-6">Click the heart icon on any dog&apos;s profile to save them here.</p>
          <Link href="/dogs" className="inline-block px-6 py-3 bg-blue-600 text-white font-medium rounded-lg hover:bg-blue-700 transition">
            Browse Dogs
          </Link>
        </div>
      ) : (
        <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-4">
          {savedDogs.map((dog) => (
            <div key={dog.id} className="bg-white rounded-lg shadow-sm border border-gray-200 overflow-hidden group relative">
              <Link href={`/dogs/${dog.id}`}>
                <div className="aspect-square bg-gray-100 relative overflow-hidden">
                  {dog.photo ? (
                    <Image src={dog.photo} alt={dog.name} fill className="object-cover group-hover:scale-105 transition" sizes="(max-width: 640px) 100vw, 25vw" />
                  ) : (
                    <div className="w-full h-full flex items-center justify-center bg-gray-200">
                      <svg className="w-16 h-16 text-gray-400" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1} d="M4 16l4.586-4.586a2 2 0 012.828 0L16 16m-2-2l1.586-1.586a2 2 0 012.828 0L20 14m-6-6h.01M6 20h12a2 2 0 002-2V6a2 2 0 00-2-2H6a2 2 0 00-2 2v12a2 2 0 002 2z" />
                      </svg>
                    </div>
                  )}
                </div>
                <div className="p-3">
                  <p className="font-bold text-gray-900 group-hover:text-blue-600 transition truncate">{dog.name}</p>
                  <p className="text-sm text-gray-500 truncate">{dog.breed}</p>
                </div>
              </Link>
              <button
                onClick={() => unsaveDog(dog.id)}
                className="absolute top-2 right-2 p-1.5 bg-white/90 rounded-full shadow hover:bg-red-50 transition"
                aria-label={`Remove ${dog.name} from saved`}
              >
                <svg className="w-5 h-5 text-red-500" fill="currentColor" viewBox="0 0 24 24">
                  <path d="M4.318 6.318a4.5 4.5 0 000 6.364L12 20.364l7.682-7.682a4.5 4.5 0 00-6.364-6.364L12 7.636l-1.318-1.318a4.5 4.5 0 00-6.364 0z" />
                </svg>
              </button>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
