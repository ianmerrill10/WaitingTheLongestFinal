"use client";

import { useState, useEffect } from "react";
import ProductCard from "./ProductCard";

interface Product {
  id: string;
  name: string;
  category: string;
  affiliate_url: string;
  image_url: string | null;
  price_min: number | null;
  price_max: number | null;
  description: string | null;
  partner_name?: string;
}

interface ProductRecommendationsProps {
  dogId: string;
  dogName: string;
  breed?: string;
  size?: string;
  ageText?: string;
  urgencyLevel?: string | null;
}

/**
 * "Prepare for [Dog Name]" section shown on dog profile pages.
 * Loads breed/size/age-matched product recommendations.
 * Hidden for urgent/euthanasia dogs per CLAUDE.md rules.
 */
export default function ProductRecommendations({
  dogId,
  dogName,
  urgencyLevel,
}: ProductRecommendationsProps) {
  const [products, setProducts] = useState<Product[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    // No affiliate content on urgent dog pages
    if (urgencyLevel === "critical" || urgencyLevel === "high") {
      setLoading(false);
      return;
    }

    async function load() {
      try {
        const res = await fetch(`/api/affiliate/click?dog_id=${dogId}`);
        if (res.ok) {
          const data = await res.json();
          setProducts(data.products || []);
        }
      } catch {
        // fail silently — recommendations are non-critical
      } finally {
        setLoading(false);
      }
    }

    load();
  }, [dogId, urgencyLevel]);

  // Don't show on urgent dogs or if no products
  if (urgencyLevel === "critical" || urgencyLevel === "high") return null;
  if (!loading && products.length === 0) return null;

  return (
    <div className="mt-8 pt-6 border-t border-gray-200">
      <h2 className="text-lg font-bold mb-1">
        Prepare for {dogName}
      </h2>
      <p className="text-xs text-gray-500 mb-4">
        These affiliate links help support our mission to save shelter dogs.
        We may earn a small commission at no extra cost to you.
      </p>

      {loading ? (
        <div className="grid grid-cols-2 sm:grid-cols-3 gap-3">
          {Array.from({ length: 4 }).map((_, i) => (
            <div
              key={i}
              className="bg-gray-100 rounded-lg h-32 animate-pulse"
            />
          ))}
        </div>
      ) : (
        <div className="grid grid-cols-2 sm:grid-cols-3 gap-3">
          {products.map((product) => (
            <ProductCard
              key={product.id}
              product={product}
              dogId={dogId}
            />
          ))}
        </div>
      )}
    </div>
  );
}
