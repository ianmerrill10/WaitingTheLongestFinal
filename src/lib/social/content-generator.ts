import { createAdminClient } from "@/lib/supabase/admin";

export type ContentType =
  | "dog_spotlight"
  | "urgent_alert"
  | "happy_tails"
  | "did_you_know"
  | "breed_facts"
  | "weekly_roundup"
  | "longest_waiting"
  | "new_arrivals"
  | "foster_appeal"
  | "shelter_spotlight";

export type Platform =
  | "tiktok"
  | "instagram"
  | "facebook"
  | "youtube"
  | "reddit"
  | "twitter"
  | "bluesky"
  | "threads"
  | "snapchat";

interface DogForContent {
  id: string;
  name: string;
  breed: string;
  age_text: string;
  sex: string;
  size: string;
  photo_url: string | null;
  photos: string[];
  shelter_name: string;
  shelter_city: string;
  shelter_state: string;
  intake_date: string;
  wait_days: number;
  urgency_level: string | null;
  euthanasia_date: string | null;
  description: string | null;
  good_with_kids: boolean | null;
  good_with_dogs: boolean | null;
  good_with_cats: boolean | null;
  external_url: string | null;
}

interface GeneratedContent {
  caption: string;
  hashtags: string[];
  media_urls: string[];
  media_type: "photo" | "carousel" | "video";
  content_type: ContentType;
  dog_id: string | null;
  shelter_id: string | null;
  platform: Platform;
}

/**
 * Select a dog based on content strategy.
 */
export async function selectDog(
  strategy: ContentType
): Promise<DogForContent | null> {
  const supabase = createAdminClient();

  let query = supabase
    .from("dogs")
    .select(
      "id, name, breed, age_text, sex, size, photo_url, photos, shelter_name, shelter_city, shelter_state, intake_date, urgency_level, euthanasia_date, description, good_with_kids, good_with_dogs, good_with_cats, external_url, shelter_id"
    )
    .eq("is_available", true);

  switch (strategy) {
    case "longest_waiting":
    case "dog_spotlight":
      query = query
        .not("intake_date", "is", null)
        .order("intake_date", { ascending: true })
        .limit(20);
      break;

    case "urgent_alert":
      query = query
        .not("euthanasia_date", "is", null)
        .in("urgency_level", ["critical", "high"])
        .order("euthanasia_date", { ascending: true })
        .limit(10);
      break;

    case "new_arrivals":
      query = query
        .gte(
          "created_at",
          new Date(Date.now() - 48 * 3600 * 1000).toISOString()
        )
        .order("created_at", { ascending: false })
        .limit(20);
      break;

    case "foster_appeal":
      query = query
        .in("urgency_level", ["critical", "high", "medium"])
        .order("intake_date", { ascending: true })
        .limit(20);
      break;

    default:
      query = query
        .not("photo_url", "is", null)
        .order("intake_date", { ascending: true })
        .limit(30);
  }

  const { data: dogs } = await query;
  if (!dogs || dogs.length === 0) return null;

  // Check which dogs haven't been posted recently
  const dogIds = dogs.map((d) => d.id);
  const thirtyDaysAgo = new Date(
    Date.now() - 30 * 24 * 3600 * 1000
  ).toISOString();

  const { data: recentPosts } = await supabase
    .from("social_posts")
    .select("dog_id")
    .in("dog_id", dogIds)
    .gte("created_at", thirtyDaysAgo);

  const recentlyPostedIds = new Set(
    (recentPosts || []).map((p) => p.dog_id)
  );
  const unpostedDogs = dogs.filter((d) => !recentlyPostedIds.has(d.id));
  const selected = unpostedDogs.length > 0 ? unpostedDogs[0] : dogs[0];

  const waitDays = selected.intake_date
    ? Math.floor(
        (Date.now() - new Date(selected.intake_date).getTime()) /
          (24 * 3600 * 1000)
      )
    : 0;

  return {
    ...selected,
    photos: selected.photos || [],
    wait_days: waitDays,
  } as DogForContent;
}

/**
 * Generate a caption for a specific platform.
 */
export function generateCaption(
  dog: DogForContent,
  platform: Platform,
  contentType: ContentType
): string {
  const dogUrl = `https://waitingthelongest.com/dogs/${dog.id}`;
  const shelterLocation = `${dog.shelter_city}, ${dog.shelter_state}`;

  const waitText =
    dog.wait_days > 365
      ? `${Math.floor(dog.wait_days / 365)} years`
      : dog.wait_days > 30
        ? `${Math.floor(dog.wait_days / 30)} months`
        : `${dog.wait_days} days`;

  // Urgent prefix
  const isUrgent =
    dog.urgency_level === "critical" || dog.urgency_level === "high";
  const urgentPrefix = isUrgent ? "URGENT: " : "";

  switch (platform) {
    case "twitter":
      return generateTwitterCaption(
        dog,
        contentType,
        urgentPrefix,
        waitText,
        shelterLocation,
        dogUrl
      );
    case "instagram":
      return generateInstagramCaption(
        dog,
        contentType,
        urgentPrefix,
        waitText,
        shelterLocation,
        dogUrl
      );
    case "facebook":
      return generateFacebookCaption(
        dog,
        contentType,
        urgentPrefix,
        waitText,
        shelterLocation,
        dogUrl
      );
    case "tiktok":
      return generateTikTokCaption(
        dog,
        contentType,
        urgentPrefix,
        waitText,
        shelterLocation
      );
    default:
      return generateDefaultCaption(
        dog,
        contentType,
        urgentPrefix,
        waitText,
        shelterLocation,
        dogUrl
      );
  }
}

function generateTwitterCaption(
  dog: DogForContent,
  contentType: ContentType,
  urgentPrefix: string,
  waitText: string,
  location: string,
  url: string
): string {
  switch (contentType) {
    case "urgent_alert":
      return `${urgentPrefix}${dog.name} needs a home NOW. This ${dog.breed} in ${location} is running out of time. Please share.\n\n${url}`;
    case "longest_waiting":
      return `${dog.name} has been waiting ${waitText} for a family. This ${dog.breed} in ${location} is one of the longest-waiting dogs in America.\n\n${url}`;
    case "new_arrivals":
      return `Meet ${dog.name}! This ${dog.breed} just arrived at a shelter in ${location} and is looking for a loving home.\n\n${url}`;
    default:
      return `${urgentPrefix}${dog.name} is a ${dog.age_text} ${dog.breed} waiting ${waitText} in ${location}. Can you help?\n\n${url}`;
  }
}

function generateInstagramCaption(
  dog: DogForContent,
  contentType: ContentType,
  urgentPrefix: string,
  waitText: string,
  location: string,
  url: string
): string {
  switch (contentType) {
    case "urgent_alert":
      return `${urgentPrefix}${dog.name} is running out of time.\n\nThis beautiful ${dog.breed} in ${location} needs a hero. ${dog.name} has been waiting ${waitText} and desperately needs someone to give them a second chance at life.\n\nEvery share could save a life. Tag someone who could help.\n\nLink in bio or visit: ${url}`;
    case "longest_waiting":
      return `${waitText}. That's how long ${dog.name} has been waiting.\n\nThis incredible ${dog.breed} has been overlooked at a shelter in ${location} for far too long. ${dog.name} deserves a family just as much as any other dog.\n\nWill you be the one to change everything?\n\nLink in bio or visit: ${url}`;
    default:
      return `Meet ${dog.name} — a ${dog.age_text} ${dog.breed} in ${location}.\n\n${dog.name} has been waiting ${waitText} for someone to say "you're coming home." Could that someone be you?\n\nLink in bio or visit: ${url}`;
  }
}

function generateFacebookCaption(
  dog: DogForContent,
  contentType: ContentType,
  urgentPrefix: string,
  waitText: string,
  location: string,
  url: string
): string {
  const goodWith = [];
  if (dog.good_with_kids) goodWith.push("kids");
  if (dog.good_with_dogs) goodWith.push("other dogs");
  if (dog.good_with_cats) goodWith.push("cats");
  const goodWithText =
    goodWith.length > 0 ? `\n\nGood with: ${goodWith.join(", ")}` : "";

  switch (contentType) {
    case "urgent_alert":
      return `${urgentPrefix}PLEASE SHARE — ${dog.name} needs a home immediately.\n\n${dog.name} is a ${dog.age_text} ${dog.breed} at a shelter in ${location}. After waiting ${waitText}, ${dog.name} is running out of time and needs your help.\n\nEvery single share matters. You might not be able to adopt, but someone you know might be the perfect match.${goodWithText}\n\nLearn more: ${url}\n\n#AdoptDontShop #SaveALife #${dog.shelter_state}Dogs`;
    default:
      return `${urgentPrefix}${dog.name} has been waiting ${waitText} for a family.\n\n${dog.name} is a ${dog.age_text} ${dog.breed} at ${dog.shelter_name} in ${location}. While most dogs find homes quickly, ${dog.name} has been overlooked — and we think that's a mistake.${goodWithText}\n\nCan you share this post? It only takes one person to change ${dog.name}'s world.\n\nMeet ${dog.name}: ${url}`;
  }
}

function generateTikTokCaption(
  dog: DogForContent,
  contentType: ContentType,
  urgentPrefix: string,
  waitText: string,
  location: string
): string {
  switch (contentType) {
    case "urgent_alert":
      return `${urgentPrefix}${dog.name} is running out of time ${location}`;
    case "longest_waiting":
      return `${dog.name} has been waiting ${waitText}. Still no family.`;
    default:
      return `${urgentPrefix}Meet ${dog.name} — ${waitText} in a shelter`;
  }
}

function generateDefaultCaption(
  dog: DogForContent,
  contentType: ContentType,
  urgentPrefix: string,
  waitText: string,
  location: string,
  url: string
): string {
  return `${urgentPrefix}${dog.name} is a ${dog.age_text} ${dog.breed} who has been waiting ${waitText} at a shelter in ${location}. Help ${dog.name} find a home.\n\n${url}`;
}

/**
 * Generate hashtags based on content type and dog details.
 */
export function generateHashtags(
  dog: DogForContent,
  platform: Platform,
  contentType: ContentType
): string[] {
  const base = [
    "AdoptDontShop",
    "RescueDog",
    "ShelterDog",
    "WaitingTheLongest",
    "AdoptAShelterDog",
  ];

  // Breed-specific
  const breedTag = dog.breed
    .replace(/[^a-zA-Z]/g, "")
    .replace(/\s+/g, "");
  if (breedTag) base.push(breedTag);

  // Location
  base.push(`${dog.shelter_state}Dogs`);

  // Content-type specific
  switch (contentType) {
    case "urgent_alert":
      base.push("Urgent", "SaveALife", "TimeIsRunningOut", "UrgentFoster");
      break;
    case "longest_waiting":
      base.push("LongestWaiting", "Overlooked", "StillWaiting");
      break;
    case "new_arrivals":
      base.push("NewArrival", "JustListed", "AvailableNow");
      break;
    case "foster_appeal":
      base.push("FosterSavesLives", "FosterDog", "TemporaryLove");
      break;
    case "happy_tails":
      base.push("HappyTails", "Adopted", "ForeverHome", "AdoptionStory");
      break;
  }

  // Platform limits
  const maxHashtags = platform === "instagram" ? 30 : 10;
  return base.slice(0, maxHashtags);
}

/**
 * Generate full social content for a platform.
 */
export async function generateContent(
  platform: Platform,
  contentType: ContentType
): Promise<GeneratedContent | null> {
  const dog = await selectDog(contentType);
  if (!dog) return null;

  const caption = generateCaption(dog, platform, contentType);
  const hashtags = generateHashtags(dog, platform, contentType);
  const media_urls = dog.photo_url
    ? [dog.photo_url, ...dog.photos.slice(0, 3)]
    : dog.photos.slice(0, 4);

  return {
    caption,
    hashtags,
    media_urls,
    media_type: media_urls.length > 1 ? "carousel" : "photo",
    content_type: contentType,
    dog_id: dog.id,
    shelter_id: null,
    platform,
  };
}
