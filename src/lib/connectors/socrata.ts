/**
 * Socrata Open Data API connector.
 * Fetches animal intake data from government open data portals.
 * These provide GOLD STANDARD intake dates (official government records).
 *
 * Socrata API docs: https://dev.socrata.com/docs/queries/
 * SoQL query language for filtering, ordering, pagination.
 */

import type { GovDataSource } from "@/data/gov-open-data-sources";

export interface GovDogRecord {
  source: string;
  sourceId: string;
  name: string;
  breed: string | null;
  sex: string | null;
  age: string | null;
  color: string | null;
  intakeDate: string; // ISO date
  intakeType: string | null;
  city: string;
  state: string;
  orgName: string;
}

/**
 * Fetch currently-in-shelter dogs from a Socrata-powered open data portal.
 * Uses SoQL to filter for dogs with intake but no outcome (still in shelter).
 */
export async function fetchSocrataAnimals(
  source: GovDataSource,
  limit: number = 5000
): Promise<GovDogRecord[]> {
  const baseUrl = `https://${source.domain}/resource/${source.datasetId}.json`;
  const f = source.fields;

  // Build SoQL WHERE clause
  const whereClauses: string[] = [];

  // Filter for dogs
  whereClauses.push(`${f.animalType}='${source.dogTypeValue}'`);

  // Filter for animals still in shelter (no outcome)
  if (f.outcomeType) {
    whereClauses.push(`${f.outcomeType} IS NULL`);
  }

  const whereClause = whereClauses.join(" AND ");

  // Build select fields
  const selectFields = [
    f.animalId,
    f.name,
    f.breed,
    f.intakeDate,
    f.animalType,
    f.sex,
    f.age,
    f.color,
    f.intakeType,
    f.outcomeType,
  ]
    .filter(Boolean)
    .join(",");

  const params = new URLSearchParams({
    $where: whereClause,
    $select: selectFields,
    $order: `${f.intakeDate} ASC`,
    $limit: String(limit),
  });

  const url = `${baseUrl}?${params.toString()}`;

  try {
    const response = await fetch(url, {
      headers: {
        Accept: "application/json",
        "User-Agent": "WaitingTheLongest/1.0 (animal-welfare; https://waitingthelongest.com)",
      },
      next: { revalidate: 3600 }, // Cache for 1 hour
    });

    if (!response.ok) {
      console.error(
        `[Socrata] ${source.name} HTTP ${response.status}: ${response.statusText}`
      );
      return [];
    }

    const records = await response.json();
    if (!Array.isArray(records)) return [];

    return records
      .map((r: Record<string, string>) => normalizeRecord(r, source))
      .filter((r): r is GovDogRecord => r !== null);
  } catch (err) {
    console.error(`[Socrata] ${source.name} fetch error:`, err);
    return [];
  }
}

function normalizeRecord(
  record: Record<string, string>,
  source: GovDataSource
): GovDogRecord | null {
  const f = source.fields;

  const rawDate = record[f.intakeDate];
  if (!rawDate) return null;

  // Parse Socrata date formats: ISO, floating timestamps, MM/DD/YYYY
  const intakeDate = parseSocrataDate(rawDate);
  if (!intakeDate) return null;

  // Validate: not in the future, not before 2000
  const parsed = new Date(intakeDate);
  if (parsed > new Date() || parsed.getFullYear() < 2000) return null;

  const name = (record[f.name] || "").trim();
  if (!name || name.length < 1) return null;

  return {
    source: `gov_opendata:${source.name.toLowerCase().replace(/\s+/g, "_")}`,
    sourceId: record[f.animalId] || "",
    name,
    breed: record[f.breed] || null,
    sex: normalizeSex(record[f.sex || ""]),
    age: record[f.age || ""] || null,
    color: record[f.color || ""] || null,
    intakeDate,
    intakeType: record[f.intakeType || ""] || null,
    city: source.city,
    state: source.state,
    orgName: source.name,
  };
}

function parseSocrataDate(raw: string): string | null {
  if (!raw) return null;

  // ISO format: "2024-01-15T00:00:00.000"
  if (raw.includes("T")) {
    const d = new Date(raw.replace("Z", "+00:00"));
    if (!isNaN(d.getTime())) return d.toISOString().split("T")[0];
  }

  // Date only: "2024-01-15"
  if (/^\d{4}-\d{2}-\d{2}$/.test(raw)) return raw;

  // MM/DD/YYYY
  const slashMatch = /^(\d{1,2})\/(\d{1,2})\/(\d{4})$/.exec(raw);
  if (slashMatch) {
    const [, m, d, y] = slashMatch;
    return `${y}-${m.padStart(2, "0")}-${d.padStart(2, "0")}`;
  }

  // Try generic parse
  const d = new Date(raw);
  if (!isNaN(d.getTime())) return d.toISOString().split("T")[0];

  return null;
}

function normalizeSex(val: string | undefined): string | null {
  if (!val) return null;
  const v = val.toLowerCase();
  if (v.includes("female")) return "female";
  if (v.includes("male")) return "male";
  if (v === "m") return "male";
  if (v === "f") return "female";
  return null;
}

/**
 * Discover new Socrata datasets with animal shelter data.
 * Uses the Socrata Discovery API to find datasets we haven't cataloged yet.
 */
export async function discoverSocrataDatasets(): Promise<
  Array<{
    name: string;
    domain: string;
    datasetId: string;
    description: string;
    columns: string[];
    updatedAt: string;
  }>
> {
  const searchTerms = [
    "animal shelter intake",
    "animal services intake",
    "animal control intake outcomes",
  ];

  const allDatasets: Array<{
    name: string;
    domain: string;
    datasetId: string;
    description: string;
    columns: string[];
    updatedAt: string;
  }> = [];
  const seen = new Set<string>();

  for (const term of searchTerms) {
    try {
      const params = new URLSearchParams({
        q: term,
        categories: "Government",
        only: "datasets",
        limit: "50",
      });

      const resp = await fetch(
        `https://api.us.socrata.com/api/catalog/v1?${params.toString()}`
      );
      if (!resp.ok) continue;

      const data = await resp.json();
      for (const result of data.results || []) {
        const resource = result.resource || {};
        const id = resource.id;
        if (!id || seen.has(id)) continue;
        seen.add(id);

        allDatasets.push({
          name: resource.name || "",
          domain: result.metadata?.domain || "",
          datasetId: id,
          description: (resource.description || "").substring(0, 500),
          columns: resource.columns_field_name || [],
          updatedAt: resource.updatedAt || "",
        });
      }
    } catch (err) {
      console.error(`[SocrataDiscovery] Search error for "${term}":`, err);
    }
  }

  return allDatasets;
}
