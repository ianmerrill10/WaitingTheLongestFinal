"use client";

import { useState, useEffect, useCallback } from "react";

const STORAGE_KEY = "wtl_recently_viewed";
const MAX_ITEMS = 20;

interface RecentDog {
  id: string;
  name: string;
  breed: string;
  photo: string | null;
  viewedAt: number;
}

export function useRecentlyViewed() {
  const [recentDogs, setRecentDogs] = useState<RecentDog[]>([]);

  useEffect(() => {
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      if (stored) setRecentDogs(JSON.parse(stored));
    } catch {}
  }, []);

  const addDog = useCallback((dog: { id: string; name: string; breed_primary: string; primary_photo_url: string | null }) => {
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      let list: RecentDog[] = stored ? JSON.parse(stored) : [];
      list = list.filter(d => d.id !== dog.id);
      list.unshift({ id: dog.id, name: dog.name, breed: dog.breed_primary, photo: dog.primary_photo_url, viewedAt: Date.now() });
      if (list.length > MAX_ITEMS) list = list.slice(0, MAX_ITEMS);
      localStorage.setItem(STORAGE_KEY, JSON.stringify(list));
      setRecentDogs(list);
    } catch {}
  }, []);

  return { recentDogs, addDog };
}
