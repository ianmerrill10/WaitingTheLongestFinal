"use client";

import { useState, useEffect } from "react";

interface SaveButtonProps {
  dogId: string;
  dogName: string;
  breed: string;
  photo: string | null;
  className?: string;
}

const STORAGE_KEY = "wtl_saved_dogs";

export default function SaveButton({ dogId, dogName, breed, photo, className = "" }: SaveButtonProps) {
  const [saved, setSaved] = useState(false);

  useEffect(() => {
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      if (stored) {
        const list = JSON.parse(stored);
        setSaved(list.some((d: { id: string }) => d.id === dogId));
      }
    } catch {}
  }, [dogId]);

  function toggle(e: React.MouseEvent) {
    e.preventDefault();
    e.stopPropagation();
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      let list = stored ? JSON.parse(stored) : [];
      if (saved) {
        list = list.filter((d: { id: string }) => d.id !== dogId);
      } else {
        list.unshift({ id: dogId, name: dogName, breed, photo, savedAt: Date.now() });
      }
      localStorage.setItem(STORAGE_KEY, JSON.stringify(list));
      setSaved(!saved);
    } catch {}
  }

  return (
    <button
      onClick={toggle}
      className={`p-1.5 rounded-full transition ${saved ? "bg-red-50 text-red-500" : "bg-white/80 text-gray-400 hover:text-red-400 hover:bg-white"} ${className}`}
      aria-label={saved ? `Remove ${dogName} from saved` : `Save ${dogName}`}
    >
      <svg className="w-5 h-5" fill={saved ? "currentColor" : "none"} viewBox="0 0 24 24" stroke="currentColor" strokeWidth={saved ? 0 : 2}>
        <path strokeLinecap="round" strokeLinejoin="round" d="M4.318 6.318a4.5 4.5 0 000 6.364L12 20.364l7.682-7.682a4.5 4.5 0 00-6.364-6.364L12 7.636l-1.318-1.318a4.5 4.5 0 00-6.364 0z" />
      </svg>
    </button>
  );
}
