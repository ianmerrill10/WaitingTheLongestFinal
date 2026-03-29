/**
 * Registry of government open data portals with animal shelter intake data.
 * These are Socrata-powered datasets that provide GOLD STANDARD intake dates.
 * Confidence level: 0.95+ (official government records).
 *
 * To add a new source:
 * 1. Find the dataset on the portal
 * 2. Map the field names to our schema
 * 3. Add to GOV_OPEN_DATA_SOURCES array
 * 4. The Socrata connector will auto-fetch dogs
 */

export interface GovDataSource {
  name: string;
  state: string;
  city: string;
  domain: string;
  datasetId: string;
  /** Platform: socrata, arcgis, ckan */
  platform: "socrata" | "arcgis" | "ckan";
  fields: {
    animalId: string;
    name: string;
    breed: string;
    intakeDate: string;
    animalType: string;
    outcomeType?: string;
    sex?: string;
    age?: string;
    color?: string;
    intakeType?: string;
  };
  /** Filter value for animal_type field to get dogs only */
  dogTypeValue: string;
  /** If true, needs cross-reference with outcomes to filter adopted */
  needsOutcomesCrossRef: boolean;
  /** Separate outcomes dataset ID (if needed) */
  outcomesDatasetId?: string;
  /** Active flag — disable sources that break or get moved */
  active: boolean;
}

export const GOV_OPEN_DATA_SOURCES: GovDataSource[] = [
  {
    name: "Austin Animal Center",
    state: "TX",
    city: "Austin",
    domain: "data.austintexas.gov",
    datasetId: "wter-evkm", // Intakes dataset (9t4d-g238 is outcomes)
    platform: "socrata",
    fields: {
      animalId: "animal_id",
      name: "name",
      breed: "breed",
      intakeDate: "datetime",
      animalType: "animal_type",
      sex: "sex_upon_intake",
      age: "age_upon_intake",
      color: "color",
      intakeType: "intake_type",
      // No outcomeType — intakes-only dataset
    },
    dogTypeValue: "Dog",
    needsOutcomesCrossRef: true,
    outcomesDatasetId: "9t4d-g238",
    active: true,
  },
  {
    name: "LA Animal Services",
    state: "CA",
    city: "Los Angeles",
    domain: "data.lacity.org",
    datasetId: "8cmr-fbcu",
    platform: "socrata",
    fields: {
      animalId: "animal_id",
      name: "animal_id", // No name field in this dataset
      breed: "breed_1",
      intakeDate: "intake_date",
      animalType: "animal_type",
      // No outcome_type, sex, age in this dataset
    },
    dogTypeValue: "DOG",
    needsOutcomesCrossRef: true,
    active: false, // No dog names in dataset — records get filtered out
  },
  {
    name: "Denver Animal Shelter",
    state: "CO",
    city: "Denver",
    domain: "data.denvergov.org",
    datasetId: "tqvi-tpzi",
    platform: "socrata",
    fields: {
      animalId: "animal_id",
      name: "animal_name",
      breed: "breed",
      intakeDate: "intake_date",
      animalType: "species",
      outcomeType: "outcome_type",
      sex: "sex",
    },
    dogTypeValue: "Dog",
    needsOutcomesCrossRef: false,
    active: false, // Dataset returns empty — may be removed or restructured
  },
  {
    name: "Louisville Metro Animal Services",
    state: "KY",
    city: "Louisville",
    domain: "data.louisvilleky.gov",
    datasetId: "mfnp-7nse",
    platform: "socrata",
    fields: {
      animalId: "animal_id",
      name: "animal_name",
      breed: "breed",
      intakeDate: "intake_date",
      animalType: "animal_type",
      outcomeType: "outcome_type",
      sex: "sex",
      age: "age",
    },
    dogTypeValue: "Dog",
    needsOutcomesCrossRef: false,
    active: false, // Redirects to ArcGIS hub — no longer Socrata
  },
  {
    name: "Sonoma County Animal Services",
    state: "CA",
    city: "Santa Rosa",
    domain: "data.sonomacounty.ca.gov",
    datasetId: "924a-vesw",
    platform: "socrata",
    fields: {
      animalId: "id",
      name: "id", // No dog name field — only has ID, breed, impound_number
      breed: "breed",
      intakeDate: "intake_date",
      animalType: "type",
      outcomeType: "outcome_type",
      sex: "sex",
      color: "color",
    },
    dogTypeValue: "DOG",
    needsOutcomesCrossRef: false,
    active: false, // No dog names in dataset — records get filtered out
  },
  {
    name: "Bloomington Animal Shelter",
    state: "IN",
    city: "Bloomington",
    domain: "data.bloomington.in.gov",
    datasetId: "e245-r9ub",
    platform: "socrata",
    fields: {
      animalId: "sheltercode",
      name: "animalname",
      breed: "breedname",
      intakeDate: "intakedate",
      animalType: "speciesname",
      sex: "sexname",
      color: "basecolour",
      // No outcomeType — uses movementtype/movementdate instead
    },
    dogTypeValue: "Dog",
    needsOutcomesCrossRef: true,
    active: true,
  },
  {
    name: "Norfolk Animal Care Center",
    state: "VA",
    city: "Norfolk",
    domain: "data.norfolk.gov",
    datasetId: "vfm4-5wv6",
    platform: "socrata",
    fields: {
      animalId: "animal_id",
      name: "animal_name",
      breed: "primary_breed",
      intakeDate: "intake_date",
      animalType: "animal_type",
      outcomeType: "outcome_type",
      sex: "sex",
      color: "primary_color",
      intakeType: "intake_type",
    },
    dogTypeValue: "Dog",
    needsOutcomesCrossRef: false,
    active: true,
  },
  {
    name: "Dallas Animal Services",
    state: "TX",
    city: "Dallas",
    domain: "www.dallasopendata.com",
    datasetId: "7h2m-czeh",
    platform: "socrata",
    fields: {
      animalId: "animal_id",
      name: "animal_type",
      breed: "animal_breed",
      intakeDate: "intake_date",
      animalType: "animal_type",
      outcomeType: "outcome_type",
      sex: "intake_condition",
    },
    dogTypeValue: "DOG",
    needsOutcomesCrossRef: false,
    active: false, // Dataset missing (404) as of 2026-03-29
  },
];

/**
 * Get all active government data sources, optionally filtered by state.
 */
export function getActiveGovSources(state?: string): GovDataSource[] {
  return GOV_OPEN_DATA_SOURCES.filter(
    (s) => s.active && (!state || s.state === state.toUpperCase())
  );
}
