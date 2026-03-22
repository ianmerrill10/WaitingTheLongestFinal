"use client";

import { useState, useEffect } from "react";
import Image from "next/image";

interface PhotoLightboxProps {
  photos: string[];
  dogName: string;
  initialIndex?: number;
}

export default function PhotoLightbox({ photos, dogName, initialIndex = 0 }: PhotoLightboxProps) {
  const [isOpen, setIsOpen] = useState(false);
  const [currentIndex, setCurrentIndex] = useState(initialIndex);

  useEffect(() => {
    if (!isOpen) return;
    function handleKey(e: KeyboardEvent) {
      if (e.key === "Escape") setIsOpen(false);
      if (e.key === "ArrowRight") setCurrentIndex(i => (i + 1) % photos.length);
      if (e.key === "ArrowLeft") setCurrentIndex(i => (i - 1 + photos.length) % photos.length);
    }
    document.addEventListener("keydown", handleKey);
    document.body.style.overflow = "hidden";
    return () => {
      document.removeEventListener("keydown", handleKey);
      document.body.style.overflow = "";
    };
  }, [isOpen, photos.length]);

  if (photos.length === 0) return null;

  return (
    <>
      {/* Thumbnail strip */}
      <div className="flex gap-2 overflow-x-auto pb-2">
        {photos.map((photo, i) => (
          <button
            key={i}
            onClick={() => { setCurrentIndex(i); setIsOpen(true); }}
            className={`w-20 h-20 flex-shrink-0 rounded-lg overflow-hidden relative bg-gray-100 hover:ring-2 hover:ring-blue-400 transition ${i === currentIndex && isOpen ? 'ring-2 ring-blue-500' : ''}`}
          >
            <Image src={photo} alt={`${dogName} photo ${i + 1}`} fill className="object-cover" sizes="80px" />
          </button>
        ))}
      </div>

      {/* Lightbox overlay */}
      {isOpen && (
        <div className="fixed inset-0 z-[100] bg-black/90 flex items-center justify-center" onClick={() => setIsOpen(false)}>
          {/* Close button */}
          <button onClick={() => setIsOpen(false)} className="absolute top-4 right-4 text-white/80 hover:text-white z-10 p-2" aria-label="Close">
            <svg className="w-8 h-8" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" /></svg>
          </button>

          {/* Previous button */}
          {photos.length > 1 && (
            <button onClick={(e) => { e.stopPropagation(); setCurrentIndex(i => (i - 1 + photos.length) % photos.length); }} className="absolute left-4 text-white/80 hover:text-white z-10 p-2" aria-label="Previous">
              <svg className="w-10 h-10" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M15 19l-7-7 7-7" /></svg>
            </button>
          )}

          {/* Image */}
          <div className="relative w-full h-full max-w-5xl max-h-[85vh] m-8" onClick={(e) => e.stopPropagation()}>
            <Image src={photos[currentIndex]} alt={`${dogName} photo ${currentIndex + 1}`} fill className="object-contain" sizes="100vw" priority />
          </div>

          {/* Next button */}
          {photos.length > 1 && (
            <button onClick={(e) => { e.stopPropagation(); setCurrentIndex(i => (i + 1) % photos.length); }} className="absolute right-4 text-white/80 hover:text-white z-10 p-2" aria-label="Next">
              <svg className="w-10 h-10" fill="none" viewBox="0 0 24 24" stroke="currentColor"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 5l7 7-7 7" /></svg>
            </button>
          )}

          {/* Counter */}
          <div className="absolute bottom-4 left-1/2 -translate-x-1/2 text-white/70 text-sm">
            {currentIndex + 1} / {photos.length}
          </div>
        </div>
      )}
    </>
  );
}
