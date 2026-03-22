import { createAdminClient } from "@/lib/supabase/admin";

const AMAZON_TAG = "waitingthelon-20";

interface AffiliatePartner {
  id: string;
  name: string;
  slug: string;
  base_url: string;
  tracking_param: string;
  affiliate_id: string;
  is_active: boolean;
}

interface AffiliateProduct {
  id: string;
  partner_id: string;
  name: string;
  category: string;
  affiliate_url: string;
  image_url: string | null;
  price_min: number | null;
  price_max: number | null;
  description: string | null;
  target_sizes: string[] | null;
  target_ages: string[] | null;
  target_breeds: string[] | null;
  priority: number;
  is_active: boolean;
}

/**
 * Get active affiliate partners.
 */
export async function getActivePartners(): Promise<AffiliatePartner[]> {
  const supabase = createAdminClient();
  const { data } = await supabase
    .from("affiliate_partners")
    .select("*")
    .eq("is_active", true)
    .order("name");

  return (data || []) as AffiliatePartner[];
}

/**
 * Get products matching a dog's attributes.
 */
export async function getProductsForDog(dog: {
  breed?: string;
  size?: string;
  age_text?: string;
}): Promise<AffiliateProduct[]> {
  const supabase = createAdminClient();

  let query = supabase
    .from("affiliate_products")
    .select("*")
    .eq("is_active", true)
    .order("priority", { ascending: false })
    .limit(12);

  const { data } = await query;
  if (!data || data.length === 0) return [];

  // Score and sort by relevance to dog
  const scored = (data as AffiliateProduct[]).map((product) => {
    let score = product.priority;

    // Size match bonus
    if (product.target_sizes && dog.size) {
      const dogSize = dog.size.toLowerCase();
      if (
        product.target_sizes.some((s) =>
          dogSize.includes(s.toLowerCase())
        )
      ) {
        score += 20;
      }
    }

    // Breed match bonus
    if (product.target_breeds && dog.breed) {
      const dogBreed = dog.breed.toLowerCase();
      if (
        product.target_breeds.some((b) =>
          dogBreed.includes(b.toLowerCase())
        )
      ) {
        score += 30;
      }
    }

    // Age match bonus
    if (product.target_ages && dog.age_text) {
      const ageText = dog.age_text.toLowerCase();
      const ageCategory = ageText.includes("puppy")
        ? "puppy"
        : ageText.includes("senior")
          ? "senior"
          : ageText.includes("young")
            ? "young"
            : "adult";
      if (product.target_ages.includes(ageCategory)) {
        score += 15;
      }
    }

    return { ...product, _score: score };
  });

  scored.sort((a, b) => b._score - a._score);
  return scored.slice(0, 6);
}

/**
 * Build an Amazon affiliate URL.
 */
export function buildAmazonUrl(
  asin: string,
  tag: string = AMAZON_TAG
): string {
  return `https://www.amazon.com/dp/${asin}?tag=${tag}`;
}

/**
 * Build an affiliate URL with tracking params.
 */
export function buildAffiliateUrl(
  baseUrl: string,
  trackingParam: string,
  affiliateId: string
): string {
  const url = new URL(baseUrl);
  url.searchParams.set(trackingParam, affiliateId);
  return url.toString();
}

/**
 * Seed initial affiliate partners.
 */
export const INITIAL_PARTNERS = [
  {
    name: "Amazon",
    slug: "amazon",
    program_type: "amazon_associates",
    affiliate_id: AMAZON_TAG,
    commission_rate: 4.0,
    cookie_duration_days: 1,
    base_url: "https://www.amazon.com",
    tracking_param: "tag",
  },
  {
    name: "Chewy",
    slug: "chewy",
    program_type: "direct",
    affiliate_id: "",
    commission_rate: 6.0,
    cookie_duration_days: 15,
    base_url: "https://www.chewy.com",
    tracking_param: "ref",
  },
  {
    name: "Petco",
    slug: "petco",
    program_type: "network_shareasale",
    affiliate_id: "",
    commission_rate: 3.0,
    cookie_duration_days: 7,
    base_url: "https://www.petco.com",
    tracking_param: "aff_id",
  },
  {
    name: "Lemonade Pet Insurance",
    slug: "lemonade",
    program_type: "direct",
    affiliate_id: "",
    commission_rate: 0,
    cookie_duration_days: 30,
    base_url: "https://www.lemonade.com/pet",
    tracking_param: "ref",
  },
  {
    name: "Healthy Paws",
    slug: "healthy-paws",
    program_type: "direct",
    affiliate_id: "",
    commission_rate: 0,
    cookie_duration_days: 30,
    base_url: "https://www.healthypawspetinsurance.com",
    tracking_param: "ref",
  },
  {
    name: "BarkBox",
    slug: "barkbox",
    program_type: "direct",
    affiliate_id: "",
    commission_rate: 0,
    cookie_duration_days: 30,
    base_url: "https://www.barkbox.com",
    tracking_param: "ref",
  },
];
