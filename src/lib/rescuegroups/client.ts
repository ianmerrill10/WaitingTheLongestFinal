import { RESCUEGROUPS_API_BASE } from "@/lib/constants";

interface RescueGroupsConfig {
  apiKey: string;
  baseUrl?: string;
}

interface RescueGroupsAnimal {
  id: string;
  type: string;
  attributes: {
    name: string;
    breedPrimary: string;
    breedSecondary: string | null;
    isMixedBreed: boolean;
    ageGroup: string;
    sex: string;
    sizeGroup: string;
    colorDetails: string | null;
    descriptionText: string | null;
    pictureThumbnailUrl: string | null;
    adoptionStatus: string;
    isAltered: boolean;
    isHousetrained: boolean;
    isSpecialNeeds: boolean;
    isKidsOk: boolean | null;
    isDogsOk: boolean | null;
    isCatsOk: boolean | null;
    createdDate: string;
    url: string | null;
  };
  relationships?: {
    orgs?: {
      data: { id: string; type: string }[];
    };
    pictures?: {
      data: { id: string; type: string }[];
    };
  };
}

interface RescueGroupsResponse {
  data: RescueGroupsAnimal[];
  included?: unknown[];
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
    // Keep only requests from the last minute
    this.requestQueue = this.requestQueue.filter((t) => now - t < 60000);

    if (this.requestQueue.length >= 100) {
      const waitMs = 60000 - (now - this.requestQueue[0]);
      await new Promise((resolve) => setTimeout(resolve, waitMs));
    }

    // Also enforce 2 req/sec
    const recentRequests = this.requestQueue.filter((t) => now - t < 1000);
    if (recentRequests.length >= 2) {
      await new Promise((resolve) => setTimeout(resolve, 500));
    }

    this.requestQueue.push(Date.now());
  }

  async fetchAnimals(page: number = 1, limit: number = 250): Promise<RescueGroupsResponse> {
    await this.rateLimit();

    const response = await fetch(
      `${this.baseUrl}/public/animals/search/available/dogs?page=${page}&limit=${limit}`,
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

  async fetchAnimalById(id: string): Promise<RescueGroupsAnimal | null> {
    await this.rateLimit();

    const response = await fetch(`${this.baseUrl}/public/animals/${id}`, {
      headers: {
        Authorization: this.apiKey,
        "Content-Type": "application/vnd.api+json",
      },
    });

    if (!response.ok) return null;
    const data = await response.json();
    return data.data?.[0] || data.data || null;
  }

  async fetchOrganizations(page: number = 1): Promise<unknown> {
    await this.rateLimit();

    const response = await fetch(
      `${this.baseUrl}/public/orgs?page=${page}&limit=250`,
      {
        headers: {
          Authorization: this.apiKey,
          "Content-Type": "application/vnd.api+json",
        },
      }
    );

    if (!response.ok) {
      throw new Error(`RescueGroups orgs error: ${response.status}`);
    }

    return response.json();
  }
}

export function createRescueGroupsClient(): RescueGroupsClient {
  const apiKey = process.env.RESCUEGROUPS_API_KEY;
  if (!apiKey) {
    throw new Error("RESCUEGROUPS_API_KEY environment variable is required");
  }
  return new RescueGroupsClient({ apiKey });
}
