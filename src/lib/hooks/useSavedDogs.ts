"use client";

import { useState, useEffect, useCallback } from "react";

const STORAGE_KEY = "wtl_saved_dogs";

interface SavedDog {
  id: string;
  name: string;
  breed: string;
  photo: string | null;
  savedAt: number;
}

export function useSavedDogs() {
  const [savedDogs, setSavedDogs] = useState<SavedDog[]>([]);

  useEffect(() => {
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      if (stored) setSavedDogs(JSON.parse(stored));
    } catch {}
  }, []);

  const saveDog = useCallback((dog: { id: string; name: string; breed_primary: string; primary_photo_url: string | null }) => {
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      let list: SavedDog[] = stored ? JSON.parse(stored) : [];
      if (list.some(d => d.id === dog.id)) return; // already saved
      list.unshift({ id: dog.id, name: dog.name, breed: dog.breed_primary, photo: dog.primary_photo_url, savedAt: Date.now() });
      localStorage.setItem(STORAGE_KEY, JSON.stringify(list));
      setSavedDogs(list);
    } catch {}
  }, []);

  const unsaveDog = useCallback((dogId: string) => {
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      let list: SavedDog[] = stored ? JSON.parse(stored) : [];
      list = list.filter(d => d.id !== dogId);
      localStorage.setItem(STORAGE_KEY, JSON.stringify(list));
      setSavedDogs(list);
    } catch {}
  }, []);

  const isSaved = useCallback((dogId: string) => {
    return savedDogs.some(d => d.id === dogId);
  }, [savedDogs]);

  return { savedDogs, saveDog, unsaveDog, isSaved };
}
