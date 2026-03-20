import { RESCUEGROUPS_API_BASE } from "@/lib/constants";

interface RescueGroupsConfig {
  apiKey: string;
  baseUrl?: string;
}

// Matches the real v5 API response for animal attributes
export interface RGAnimalAttributes {
  name: string;
  breedPrimary: string;
  breedSecondary?: string | null;
  breedString?: string;
  isBreedMixed?: boolean;
  ageGroup: string;
  ageString?: string;
  sex: string;
  sizeGroup: string;
  coatLength?: string;
  descriptionText: string | null;
  descriptionHtml?: string | null;
  pictureThumbnailUrl: string | null;
  pictureCount?: number;
  adoptionFeeString?: string;
  isAdoptionPending?: boolean;
  isCurrentVaccinations?: boolean;
  isAltered?: boolean;
  isHousetrained?: boolean;
  isSpecialNeeds?: boolean;
  isKidsOk?: boolean | null;
  isDogsOk?: boolean | null;
  isCatsOk?: boolean | null;
  isNeedingFoster?: boolean;
  activityLevel?: string;
  energyLevel?: string;
  qualities?: string[];
  url: string | null;
  createdDate: string;
  updatedDate?: string;
  priority?: number;
  birthDate?: string | null;
  isBirthDateExact?: boolean | null;
  isCourtesyListing?: boolean;
}

export interface RGAnimal {
  type: string;
  id: string;
  attributes: RGAnimalAttributes;
  relationships?: {
    orgs?: { data: { id: string; type: string }[] };
    pictures?: { data: { id: string; type: string }[] };
    breeds?: { data: { id: string; type: string }[] };
    colors?: { data: { id: string; type: string }[] };
    locations?: { data: { id: string; type: string }[] };
  };
}

export interface RGOrgAttributes {
  name: string;
  city: string;
  state: string;
  postalcode?: string;
  country?: string;
  email?: string;
  url?: string;
  facebookUrl?: string;
  phone?: string;
  about?: string;
  type?: string;
  lat?: number;
  lon?: number;
  citystate?: string;
}

export interface RGIncludedItem {
  type: string;
  id: string;
  attributes: Record<string, unknown>;
}

export interface RescueGroupsResponse {
  data: RGAnimal[];
  included?: RGIncludedItem[];
  meta?: {
    count: number;
    countReturned: number;
    pageReturned: number;
    limit: number;
    pages: number;
  };
}

export class RescueGroupsClient {
  private apiKey: string;
  private baseUrl: string;
  private requestQueue: number[] = [];

  constructor(config: RescueGroupsConfig) {
    this.apiKey = config.apiKey;
    this.baseUrl = config.baseUrl || RESCUEGROUPS_API_BASE;
  }

  private async rateLimit(): Promise<void> {
    const now = Date.now();
    this.requestQueue = this.requestQueue.filter((t) => now - t < 60000);

    if (this.requestQueue.length >= 100) {
      const waitMs = 60000 - (now - this.requestQueue[0]);
      await new Promise((resolve) => setTimeout(resolve, waitMs));
    }

    const recentRequests = this.requestQueue.filter((t) => now - t < 1000);
    if (recentRequests.length >= 2) {
      await new Promise((resolve) => setTimeout(resolve, 500));
    }

    this.requestQueue.push(Date.now());
  }

  async fetchAnimals(
    page: number = 1,
    limit: number = 250
  ): Promise<RescueGroupsResponse> {
    await this.rateLimit();

    const response = await fetch(
      `${this.baseUrl}/public/animals/search/available/dogs/?page=${page}&limit=${limit}&include=orgs,pictures`,
      {
        method: "GET",
        headers: {
          Authorization: this.apiKey,
          "Content-Type": "application/vnd.api+json",
        },
      }
    );

    if (!response.ok) {
      throw new Error(
        `RescueGroups API error: ${response.status} ${response.statusText}`
      );
    }

    return response.json();
  }

  async fetchAnimalById(id: string): Promise<RGAnimal | null> {
    await this.rateLimit();

    const response = await fetch(
      `${this.baseUrl}/public/animals/${id}?include=orgs,pictures`,
      {
        headers: {
          Authorization: this.apiKey,
          "Content-Type": "application/vnd.api+json",
        },
      }
    );

    if (!response.ok) return null;
    const data = await response.json();
    return data.data?.[0] || data.data || null;
  }

  /**
   * Verify if an animal still exists and is available in RescueGroups.
   * Returns a status indicating the verification result.
   */
  async verifyAnimal(id: string): Promise<{
    status: "available" | "not_found" | "pending" | "api_error";
    animal?: RGAnimal;
    httpStatus?: number;
  }> {
    await this.rateLimit();

    try {
      const response = await fetch(
        `${this.baseUrl}/public/animals/${id}`,
        {
          headers: {
            Authorization: this.apiKey,
            "Content-Type": "application/vnd.api+json",
          },
        }
      );

      if (response.status === 404) {
        return { status: "not_found", httpStatus: 404 };
      }

      if (!response.ok) {
        return { status: "api_error", httpStatus: response.status };
      }

      const data = await response.json();
      const animal = data.data?.[0] || data.data;

      if (!animal) {
        return { status: "not_found", httpStatus: 200 };
      }

      // Check if the animal is adoption pending
      if (animal.attributes?.isAdoptionPending) {
        return { status: "pending", animal, httpStatus: 200 };
      }

      return { status: "available", animal, httpStatus: 200 };
    } catch {
      return { status: "api_error" };
    }
  }
}

export function createRescueGroupsClient(): RescueGroupsClient {
  const apiKey = process.env.RESCUEGROUPS_API_KEY;
  if (!apiKey) {
    throw new Error("RESCUEGROUPS_API_KEY environment variable is required");
  }
  return new RescueGroupsClient({ apiKey });
}
