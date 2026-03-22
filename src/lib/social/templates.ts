import { createAdminClient } from "@/lib/supabase/admin";
import type { ContentType, Platform } from "./content-generator";

interface SocialTemplate {
  id: string;
  name: string;
  platform: Platform | null;
  content_type: ContentType;
  caption_template: string;
  hashtag_sets: string[];
  media_requirements: {
    format?: string;
    dimensions?: string;
    duration?: string;
  } | null;
  is_active: boolean;
  usage_count: number;
  avg_engagement: number | null;
}

/**
 * Get templates matching criteria.
 */
export async function getTemplates(filters?: {
  platform?: Platform;
  content_type?: ContentType;
  active_only?: boolean;
}): Promise<SocialTemplate[]> {
  const supabase = createAdminClient();

  let query = supabase.from("social_templates").select("*");

  if (filters?.platform) {
    query = query.or(
      `platform.eq.${filters.platform},platform.is.null`
    );
  }
  if (filters?.content_type) {
    query = query.eq("content_type", filters.content_type);
  }
  if (filters?.active_only !== false) {
    query = query.eq("is_active", true);
  }

  query = query.order("usage_count", { ascending: false });

  const { data } = await query;
  return (data || []) as SocialTemplate[];
}

/**
 * Apply template variables.
 */
export function applyTemplate(
  template: string,
  vars: Record<string, string>
): string {
  let result = template;
  for (const [key, value] of Object.entries(vars)) {
    result = result.replace(new RegExp(`\\{\\{${key}\\}\\}`, "g"), value);
  }
  return result;
}

/**
 * Create a new template.
 */
export async function createTemplate(input: {
  name: string;
  platform: Platform | null;
  content_type: ContentType;
  caption_template: string;
  hashtag_sets?: string[];
  media_requirements?: Record<string, string>;
}): Promise<SocialTemplate | null> {
  const supabase = createAdminClient();

  const { data, error } = await supabase
    .from("social_templates")
    .insert({
      name: input.name,
      platform: input.platform,
      content_type: input.content_type,
      caption_template: input.caption_template,
      hashtag_sets: input.hashtag_sets || [],
      media_requirements: input.media_requirements || null,
    })
    .select()
    .single();

  if (error) {
    console.error("Failed to create template:", error);
    return null;
  }

  return data as SocialTemplate;
}

/**
 * Update a template.
 */
export async function updateTemplate(
  id: string,
  updates: Partial<{
    name: string;
    caption_template: string;
    hashtag_sets: string[];
    is_active: boolean;
  }>
): Promise<boolean> {
  const supabase = createAdminClient();

  const { error } = await supabase
    .from("social_templates")
    .update(updates)
    .eq("id", id);

  return !error;
}

/**
 * Seed default templates.
 */
export const DEFAULT_TEMPLATES: Array<{
  name: string;
  platform: Platform | null;
  content_type: ContentType;
  caption_template: string;
  hashtag_sets: string[];
}> = [
  {
    name: "Urgent Alert - Universal",
    platform: null,
    content_type: "urgent_alert",
    caption_template:
      "URGENT: {{dog_name}} needs a home NOW.\n\nThis {{breed}} in {{location}} has been waiting {{wait_time}} and is running out of time. Please share — it could save a life.\n\n{{url}}",
    hashtag_sets: [
      "Urgent",
      "SaveALife",
      "AdoptDontShop",
      "ShelterDog",
      "RescueDog",
    ],
  },
  {
    name: "Longest Waiting - Universal",
    platform: null,
    content_type: "longest_waiting",
    caption_template:
      "{{wait_time}}. That's how long {{dog_name}} has been waiting for a family.\n\nThis {{breed}} at {{shelter_name}} in {{location}} has been overlooked for far too long. Could you be the one to change everything?\n\n{{url}}",
    hashtag_sets: [
      "LongestWaiting",
      "Overlooked",
      "StillWaiting",
      "AdoptDontShop",
      "RescueDog",
    ],
  },
  {
    name: "Dog Spotlight - Instagram",
    platform: "instagram",
    content_type: "dog_spotlight",
    caption_template:
      "Meet {{dog_name}} — a {{age}} {{breed}} looking for love in {{location}}.\n\n{{dog_name}} has been waiting {{wait_time}} for someone to say 'you're coming home.' Could that someone be you?\n\nEvery share helps. Tag a friend who might fall in love.\n\nLink in bio or visit waitingthelongest.com",
    hashtag_sets: [
      "AdoptDontShop",
      "RescueDog",
      "ShelterDog",
      "DogOfTheDay",
      "AdoptMe",
    ],
  },
  {
    name: "Quick Alert - Twitter",
    platform: "twitter",
    content_type: "urgent_alert",
    caption_template:
      "URGENT: {{dog_name}}, a {{breed}} in {{location}}, needs a home NOW. {{wait_time}} waiting. Please RT.\n\n{{url}}",
    hashtag_sets: ["Urgent", "AdoptDontShop", "RescueDog"],
  },
  {
    name: "New Arrival - Facebook",
    platform: "facebook",
    content_type: "new_arrivals",
    caption_template:
      "NEW ARRIVAL: Say hello to {{dog_name}}!\n\nThis adorable {{age}} {{breed}} just arrived at {{shelter_name}} in {{location}} and is already looking for a forever family.\n\n{{description}}\n\nInterested in meeting {{dog_name}}? Visit: {{url}}",
    hashtag_sets: [
      "NewArrival",
      "AdoptDontShop",
      "ShelterDog",
      "AdoptMe",
    ],
  },
  {
    name: "Foster Appeal - Universal",
    platform: null,
    content_type: "foster_appeal",
    caption_template:
      "Can you open your home temporarily?\n\n{{dog_name}} is a {{breed}} in {{location}} who needs a foster home. Fostering saves lives — the shelter provides food, supplies, and vet care. You provide the love.\n\nLearn more: {{url}}",
    hashtag_sets: [
      "FosterSavesLives",
      "FosterDog",
      "TemporaryLove",
      "AdoptDontShop",
    ],
  },
];
