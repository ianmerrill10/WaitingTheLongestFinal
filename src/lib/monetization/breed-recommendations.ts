import { getRecommendationsForDog } from "@/data/breed-product-map";
import { getProductsForDog } from "./affiliate";

interface DogAttributes {
  id: string;
  name: string;
  breed?: string;
  size?: string;
  age_text?: string;
  description?: string;
  urgency_level?: string | null;
}

interface ProductRecommendation {
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

/**
 * Get product recommendations for a specific dog.
 * Returns empty array for urgent/euthanasia dogs (no ads per CLAUDE.md rules).
 */
export async function getRecommendations(
  dog: DogAttributes
): Promise<ProductRecommendation[]> {
  // No affiliate links on urgent/euthanasia dog pages
  if (
    dog.urgency_level === "critical" ||
    dog.urgency_level === "high"
  ) {
    return [];
  }

  // Get breed/size/age-based recommendation categories
  const { categories } = getRecommendationsForDog({
    breed: dog.breed,
    size: dog.size,
    age_text: dog.age_text,
    description: dog.description,
  });

  // Get matching products from database
  const products = await getProductsForDog({
    breed: dog.breed,
    size: dog.size,
    age_text: dog.age_text,
  });

  // If we have DB products, use them
  if (products.length > 0) {
    return products.map((p) => ({
      id: p.id,
      name: p.name,
      category: p.category,
      affiliate_url: p.affiliate_url,
      image_url: p.image_url,
      price_min: p.price_min,
      price_max: p.price_max,
      description: p.description,
    }));
  }

  // Fallback: generate Amazon search links from categories
  const amazonTag = "waitingthelon-20";
  return categories.slice(0, 6).map((cat, i) => ({
    id: `fallback-${i}`,
    name: formatCategoryName(cat),
    category: cat,
    affiliate_url: `https://www.amazon.com/s?k=${encodeURIComponent(cat + " dog")}&tag=${amazonTag}`,
    image_url: null,
    price_min: null,
    price_max: null,
    description: `Browse ${formatCategoryName(cat).toLowerCase()} perfect for ${dog.breed || "your new dog"}`,
    partner_name: "Amazon",
  }));
}

function formatCategoryName(category: string): string {
  return category
    .replace(/_/g, " ")
    .replace(/\b\w/g, (c) => c.toUpperCase());
}

/**
 * Get a "new dog essentials" kit recommendation.
 * These are universal products every adopter needs.
 */
export function getEssentialsKit(size?: string): ProductRecommendation[] {
  const amazonTag = "waitingthelon-20";
  const sizeModifier = size
    ? ` ${size.toLowerCase()}`
    : "";

  return [
    {
      id: "essential-food",
      name: "Dog Food",
      category: "food",
      affiliate_url: `https://www.amazon.com/s?k=${encodeURIComponent(`dog food${sizeModifier} breed`)}&tag=${amazonTag}`,
      image_url: null,
      price_min: 25,
      price_max: 60,
      description: "Quality nutrition for your new family member",
      partner_name: "Amazon",
    },
    {
      id: "essential-bed",
      name: "Dog Bed",
      category: "beds",
      affiliate_url: `https://www.amazon.com/s?k=${encodeURIComponent(`dog bed${sizeModifier}`)}&tag=${amazonTag}`,
      image_url: null,
      price_min: 20,
      price_max: 80,
      description: "A comfortable place to rest and feel safe",
      partner_name: "Amazon",
    },
    {
      id: "essential-leash",
      name: "Leash & Collar Set",
      category: "leashes",
      affiliate_url: `https://www.amazon.com/s?k=${encodeURIComponent(`dog leash collar set${sizeModifier}`)}&tag=${amazonTag}`,
      image_url: null,
      price_min: 10,
      price_max: 30,
      description: "For safe walks and outdoor adventures",
      partner_name: "Amazon",
    },
    {
      id: "essential-bowls",
      name: "Food & Water Bowls",
      category: "food_bowl",
      affiliate_url: `https://www.amazon.com/s?k=${encodeURIComponent("dog food water bowls stainless steel")}&tag=${amazonTag}`,
      image_url: null,
      price_min: 10,
      price_max: 25,
      description: "Stainless steel bowls that last a lifetime",
      partner_name: "Amazon",
    },
  ];
}
