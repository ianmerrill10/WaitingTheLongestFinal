// TypeScript interfaces for Monetization & Social Media
// Maps to migration 011: affiliate_partners, affiliate_products, affiliate_clicks,
// social_accounts, social_posts, social_templates

// =====================================================
// AFFILIATE PARTNERS
// =====================================================

export type ProgramType = 'direct' | 'network_shareasale' | 'network_cj' | 'amazon_associates';

export interface AffiliatePartner {
  id: string;
  name: string;
  slug: string;
  program_type: ProgramType | null;
  affiliate_id: string | null;
  commission_rate: number | null;
  cookie_duration_days: number | null;
  base_url: string | null;
  tracking_param: string | null;
  is_active: boolean;
  total_clicks: number;
  total_conversions: number;
  total_revenue: number;
  created_at: string;
}

// =====================================================
// AFFILIATE PRODUCTS
// =====================================================

export type ProductCategory = 'food' | 'treats' | 'toys' | 'beds' | 'crates' | 'leashes' | 'grooming' | 'health' | 'training' | 'clothing' | 'accessories' | 'insurance';
export type DogSize = 'small' | 'medium' | 'large' | 'xlarge';
export type DogAgeGroup = 'puppy' | 'young' | 'adult' | 'senior';

export interface AffiliateProduct {
  id: string;
  partner_id: string;
  name: string;
  category: ProductCategory;
  affiliate_url: string;
  image_url: string | null;
  price_min: number | null;
  price_max: number | null;
  description: string | null;
  target_sizes: DogSize[] | null;
  target_ages: DogAgeGroup[] | null;
  target_breeds: string[] | null;
  target_conditions: string[] | null;
  target_seasons: string[] | null;
  priority: number;
  is_active: boolean;
  click_count: number;
  conversion_count: number;
  revenue_total: number;
  created_at: string;
  updated_at: string;
  // Joined fields
  partner_name?: string;
  partner_slug?: string;
}

// =====================================================
// AFFILIATE CLICKS
// =====================================================

export interface AffiliateClick {
  id: string;
  product_id: string;
  partner_id: string;
  dog_id: string | null;
  page_url: string | null;
  ip_hash: string | null;
  user_agent: string | null;
  referrer: string | null;
  session_id: string | null;
  clicked_at: string;
}

export interface TrackClickInput {
  product_id: string;
  partner_id: string;
  dog_id?: string;
  page_url?: string;
  ip_hash?: string;
  user_agent?: string;
  referrer?: string;
  session_id?: string;
}

// =====================================================
// SOCIAL ACCOUNTS
// =====================================================

export type SocialPlatform = 'tiktok' | 'facebook' | 'instagram' | 'youtube' | 'reddit' | 'snapchat' | 'twitter' | 'threads' | 'bluesky';

export interface SocialAccount {
  id: string;
  platform: SocialPlatform;
  handle: string;
  display_name: string | null;
  profile_url: string | null;
  access_token: string | null;
  refresh_token: string | null;
  token_expires_at: string | null;
  is_active: boolean;
  follower_count: number;
  following_count: number;
  post_count: number;
  last_posted_at: string | null;
  last_synced_at: string | null;
  metadata: Record<string, unknown>;
  created_at: string;
  updated_at: string;
}

// =====================================================
// SOCIAL POSTS
// =====================================================

export type ContentType = 'dog_spotlight' | 'urgent_alert' | 'happy_tails' | 'did_you_know' | 'breed_facts' | 'weekly_roundup' | 'longest_waiting' | 'new_arrivals' | 'foster_appeal' | 'shelter_spotlight';
export type PostStatus = 'draft' | 'generating' | 'review' | 'scheduled' | 'posting' | 'posted' | 'failed' | 'archived';
export type MediaType = 'photo' | 'video' | 'carousel' | 'slideshow';

export interface SocialPost {
  id: string;
  account_id: string;
  platform: SocialPlatform;
  content_type: ContentType;
  dog_id: string | null;
  shelter_id: string | null;
  caption: string;
  hashtags: string[] | null;
  media_urls: string[] | null;
  media_type: MediaType | null;
  video_script: string | null;
  status: PostStatus;
  scheduled_at: string | null;
  posted_at: string | null;
  external_post_id: string | null;
  external_post_url: string | null;
  engagement: {
    likes?: number;
    comments?: number;
    shares?: number;
    views?: number;
    saves?: number;
    clicks?: number;
  };
  last_engagement_sync: string | null;
  performance_score: number | null;
  generated_by: string | null;
  reviewed_by: string | null;
  reviewed_at: string | null;
  notes: string | null;
  created_at: string;
  updated_at: string;
  // Joined
  account_handle?: string;
  dog_name?: string;
}

// =====================================================
// SOCIAL TEMPLATES
// =====================================================

export interface SocialTemplate {
  id: string;
  name: string;
  platform: SocialPlatform | null;
  content_type: ContentType;
  caption_template: string;
  hashtag_sets: string[] | null;
  media_requirements: {
    format?: string;
    dimensions?: string;
    duration?: number;
    max_size_mb?: number;
  } | null;
  is_active: boolean;
  usage_count: number;
  avg_engagement: number | null;
  created_at: string;
}

// =====================================================
// RECOMMENDATION ENGINE TYPES
// =====================================================

export interface ProductRecommendation {
  product: AffiliateProduct;
  relevance_score: number;
  match_reasons: string[];
}

export interface BreedProductMapping {
  breed: string;
  categories: ProductCategory[];
  keywords: string[];
  size_group: DogSize;
  special_needs?: string[];
}

// =====================================================
// REVENUE ANALYTICS
// =====================================================

export interface RevenueStats {
  total_clicks: number;
  total_conversions: number;
  total_revenue: number;
  conversion_rate: number;
  average_order_value: number;
}

export interface RevenuePeriodStats extends RevenueStats {
  period: string;
  start_date: string;
  end_date: string;
}

export interface PartnerRevenue {
  partner_id: string;
  partner_name: string;
  clicks: number;
  conversions: number;
  revenue: number;
  commission_rate: number;
}

export interface ProductPerformance {
  product_id: string;
  product_name: string;
  partner_name: string;
  category: ProductCategory;
  clicks: number;
  conversions: number;
  revenue: number;
  ctr: number;
}
