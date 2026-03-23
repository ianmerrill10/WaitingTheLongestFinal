/**
 * High-Volume Shelter Catalog
 * The ~50 highest-volume municipal shelters in the US.
 * These shelters produce the most euthanasia cases — targeted scrapers for each.
 * Sources: ASPCA, Best Friends, Shelter Animals Count annual reports.
 */

export interface HighVolumeShelter {
  name: string;
  city: string;
  state: string;
  /** Estimated daily dog intake */
  estimatedDailyIntake: number;
  /** Known platform or scraping approach */
  platform: "petharbor" | "chameleon" | "shelterluv" | "petango" | "shelterbuddy" | "sheltermanager" | "custom" | "unknown";
  /** Direct URL for adoptable animals */
  adoptableUrl?: string;
  /** PetHarbor shelter ID if applicable */
  petharborId?: string;
  /** Whether we have a working scraper for this shelter */
  scraperReady: boolean;
  /** Notes about custom scraping needs */
  notes?: string;
}

/**
 * Curated list of highest-volume municipal shelters.
 * Ordered by estimated daily intake (highest first).
 */
export const HIGH_VOLUME_SHELTERS: HighVolumeShelter[] = [
  // Texas
  {
    name: "Harris County Animal Shelter",
    city: "Houston",
    state: "TX",
    estimatedDailyIntake: 80,
    platform: "chameleon",
    petharborId: "HRRS",
    adoptableUrl: "https://www.countypets.com/adopt",
    scraperReady: true,
  },
  {
    name: "Dallas Animal Services",
    city: "Dallas",
    state: "TX",
    estimatedDailyIntake: 60,
    platform: "chameleon",
    petharborId: "DLLS",
    adoptableUrl: "https://www.dallasgov.com/animals",
    scraperReady: true,
  },
  {
    name: "San Antonio Animal Care Services",
    city: "San Antonio",
    state: "TX",
    estimatedDailyIntake: 55,
    platform: "chameleon",
    petharborId: "SATX",
    adoptableUrl: "https://www.sanantonio.gov/Animal-Care",
    scraperReady: true,
  },
  {
    name: "Austin Animal Center",
    city: "Austin",
    state: "TX",
    estimatedDailyIntake: 40,
    platform: "shelterluv",
    adoptableUrl: "https://www.austintexas.gov/austin-animal-center",
    scraperReady: true,
  },
  {
    name: "Fort Worth Animal Care & Control",
    city: "Fort Worth",
    state: "TX",
    estimatedDailyIntake: 35,
    platform: "chameleon",
    petharborId: "FWTX",
    scraperReady: true,
  },
  {
    name: "El Paso Animal Services",
    city: "El Paso",
    state: "TX",
    estimatedDailyIntake: 30,
    platform: "petango",
    scraperReady: true,
  },
  // California
  {
    name: "Los Angeles County Animal Care",
    city: "Los Angeles",
    state: "CA",
    estimatedDailyIntake: 70,
    platform: "chameleon",
    petharborId: "LACT",
    adoptableUrl: "https://animalcare.lacounty.gov/adopt/",
    scraperReady: true,
  },
  {
    name: "LA City Animal Services",
    city: "Los Angeles",
    state: "CA",
    estimatedDailyIntake: 60,
    platform: "chameleon",
    petharborId: "LACT2",
    scraperReady: true,
  },
  {
    name: "Riverside County Animal Services",
    city: "Riverside",
    state: "CA",
    estimatedDailyIntake: 40,
    platform: "chameleon",
    petharborId: "RVRSD",
    scraperReady: true,
  },
  {
    name: "San Bernardino County Animal Care",
    city: "San Bernardino",
    state: "CA",
    estimatedDailyIntake: 35,
    platform: "chameleon",
    petharborId: "SBCO",
    scraperReady: true,
  },
  {
    name: "Kern County Animal Services",
    city: "Bakersfield",
    state: "CA",
    estimatedDailyIntake: 30,
    platform: "chameleon",
    petharborId: "KERN",
    scraperReady: true,
  },
  {
    name: "Fresno Bully Rescue / Fresno Humane",
    city: "Fresno",
    state: "CA",
    estimatedDailyIntake: 25,
    platform: "petango",
    scraperReady: true,
  },
  {
    name: "Sacramento County Animal Care",
    city: "Sacramento",
    state: "CA",
    estimatedDailyIntake: 25,
    platform: "chameleon",
    petharborId: "SACR",
    scraperReady: true,
  },
  // Florida
  {
    name: "Miami-Dade Animal Services",
    city: "Miami",
    state: "FL",
    estimatedDailyIntake: 50,
    platform: "chameleon",
    petharborId: "MIAD",
    adoptableUrl: "https://www.miamidade.gov/global/animals/home.page",
    scraperReady: true,
  },
  {
    name: "Broward County Animal Care",
    city: "Fort Lauderdale",
    state: "FL",
    estimatedDailyIntake: 35,
    platform: "chameleon",
    petharborId: "BRWD",
    scraperReady: true,
  },
  {
    name: "Orange County Animal Services",
    city: "Orlando",
    state: "FL",
    estimatedDailyIntake: 30,
    platform: "petango",
    scraperReady: true,
  },
  {
    name: "Jacksonville Animal Care & Protective Services",
    city: "Jacksonville",
    state: "FL",
    estimatedDailyIntake: 30,
    platform: "chameleon",
    petharborId: "JCKV",
    scraperReady: true,
  },
  {
    name: "Palm Beach County Animal Care",
    city: "West Palm Beach",
    state: "FL",
    estimatedDailyIntake: 25,
    platform: "chameleon",
    petharborId: "PALM",
    scraperReady: true,
  },
  // Arizona
  {
    name: "Maricopa County Animal Care",
    city: "Phoenix",
    state: "AZ",
    estimatedDailyIntake: 55,
    platform: "chameleon",
    petharborId: "MARP",
    adoptableUrl: "https://www.maricopa.gov/5655/Adopt-a-Pet",
    scraperReady: true,
  },
  {
    name: "Pima Animal Care Center",
    city: "Tucson",
    state: "AZ",
    estimatedDailyIntake: 35,
    platform: "shelterluv",
    adoptableUrl: "https://www.pima.gov/pacc",
    scraperReady: true,
  },
  // Georgia
  {
    name: "Fulton County Animal Services",
    city: "Atlanta",
    state: "GA",
    estimatedDailyIntake: 35,
    platform: "chameleon",
    petharborId: "FULT",
    scraperReady: true,
  },
  {
    name: "DeKalb County Animal Services",
    city: "Decatur",
    state: "GA",
    estimatedDailyIntake: 25,
    platform: "chameleon",
    petharborId: "DKLB",
    scraperReady: true,
  },
  // North Carolina
  {
    name: "Charlotte-Mecklenburg Animal Care",
    city: "Charlotte",
    state: "NC",
    estimatedDailyIntake: 30,
    platform: "petango",
    scraperReady: true,
  },
  {
    name: "Wake County Animal Center",
    city: "Raleigh",
    state: "NC",
    estimatedDailyIntake: 20,
    platform: "petango",
    scraperReady: true,
  },
  // New York
  {
    name: "NYC Animal Care Centers (ACC)",
    city: "New York",
    state: "NY",
    estimatedDailyIntake: 45,
    platform: "custom",
    adoptableUrl: "https://www.nycacc.org/adopt",
    scraperReady: true,
    notes: "Custom NYC ACC scraper already built (src/lib/scrapers/nyc-acc.ts)",
  },
  // Tennessee
  {
    name: "Memphis Animal Services",
    city: "Memphis",
    state: "TN",
    estimatedDailyIntake: 40,
    platform: "chameleon",
    petharborId: "MEMP",
    scraperReady: true,
  },
  {
    name: "Metro Nashville Animal Care",
    city: "Nashville",
    state: "TN",
    estimatedDailyIntake: 25,
    platform: "shelterbuddy",
    scraperReady: true,
  },
  // Louisiana
  {
    name: "Louisiana SPCA",
    city: "New Orleans",
    state: "LA",
    estimatedDailyIntake: 30,
    platform: "shelterluv",
    scraperReady: true,
  },
  {
    name: "East Baton Rouge Animal Control",
    city: "Baton Rouge",
    state: "LA",
    estimatedDailyIntake: 25,
    platform: "chameleon",
    petharborId: "EBRA",
    scraperReady: true,
  },
  // Mississippi
  {
    name: "Jackson-Hinds Humane Society",
    city: "Jackson",
    state: "MS",
    estimatedDailyIntake: 20,
    platform: "unknown",
    scraperReady: false,
    notes: "Needs manual investigation for platform detection",
  },
  // Alabama
  {
    name: "Jefferson County Animal Control",
    city: "Birmingham",
    state: "AL",
    estimatedDailyIntake: 25,
    platform: "chameleon",
    petharborId: "JFCO",
    scraperReady: true,
  },
  // South Carolina
  {
    name: "Charleston Animal Society",
    city: "Charleston",
    state: "SC",
    estimatedDailyIntake: 20,
    platform: "shelterluv",
    scraperReady: true,
  },
  {
    name: "Greenville County Animal Care",
    city: "Greenville",
    state: "SC",
    estimatedDailyIntake: 20,
    platform: "petango",
    scraperReady: true,
  },
  // Oklahoma
  {
    name: "Oklahoma City Animal Welfare",
    city: "Oklahoma City",
    state: "OK",
    estimatedDailyIntake: 30,
    platform: "chameleon",
    petharborId: "OKCW",
    scraperReady: true,
  },
  // Missouri
  {
    name: "Kansas City Pet Project",
    city: "Kansas City",
    state: "MO",
    estimatedDailyIntake: 25,
    platform: "shelterluv",
    scraperReady: true,
  },
  {
    name: "St. Louis Animal Care",
    city: "St. Louis",
    state: "MO",
    estimatedDailyIntake: 20,
    platform: "chameleon",
    petharborId: "STLS",
    scraperReady: true,
  },
  // Ohio
  {
    name: "Franklin County Dog Shelter",
    city: "Columbus",
    state: "OH",
    estimatedDailyIntake: 25,
    platform: "chameleon",
    petharborId: "FRNK",
    scraperReady: true,
  },
  {
    name: "Cuyahoga County Animal Shelter",
    city: "Cleveland",
    state: "OH",
    estimatedDailyIntake: 20,
    platform: "petango",
    scraperReady: true,
  },
  // Michigan
  {
    name: "Detroit Animal Care",
    city: "Detroit",
    state: "MI",
    estimatedDailyIntake: 25,
    platform: "chameleon",
    petharborId: "DTRT",
    scraperReady: true,
  },
  // Indiana
  {
    name: "Indianapolis Animal Care Services",
    city: "Indianapolis",
    state: "IN",
    estimatedDailyIntake: 30,
    platform: "chameleon",
    petharborId: "INDY",
    scraperReady: true,
  },
  // Nevada
  {
    name: "The Animal Foundation",
    city: "Las Vegas",
    state: "NV",
    estimatedDailyIntake: 40,
    platform: "shelterluv",
    adoptableUrl: "https://animalfoundation.com/adopt",
    scraperReady: true,
  },
  // Colorado
  {
    name: "Denver Animal Protection",
    city: "Denver",
    state: "CO",
    estimatedDailyIntake: 20,
    platform: "petango",
    scraperReady: true,
  },
  // New Mexico
  {
    name: "Albuquerque Animal Welfare",
    city: "Albuquerque",
    state: "NM",
    estimatedDailyIntake: 25,
    platform: "chameleon",
    petharborId: "ABQN",
    scraperReady: true,
  },
  // Virginia
  {
    name: "Richmond Animal Care & Control",
    city: "Richmond",
    state: "VA",
    estimatedDailyIntake: 20,
    platform: "shelterluv",
    scraperReady: true,
  },
  // Maryland
  {
    name: "Baltimore Animal Rescue & Care Shelter",
    city: "Baltimore",
    state: "MD",
    estimatedDailyIntake: 20,
    platform: "shelterbuddy",
    scraperReady: true,
  },
  // Kentucky
  {
    name: "Louisville Metro Animal Services",
    city: "Louisville",
    state: "KY",
    estimatedDailyIntake: 25,
    platform: "chameleon",
    petharborId: "LVKY",
    scraperReady: true,
  },
  // Arkansas
  {
    name: "Little Rock Animal Village",
    city: "Little Rock",
    state: "AR",
    estimatedDailyIntake: 20,
    platform: "unknown",
    scraperReady: false,
    notes: "Needs manual investigation",
  },
  // Washington
  {
    name: "Seattle Animal Shelter",
    city: "Seattle",
    state: "WA",
    estimatedDailyIntake: 15,
    platform: "shelterluv",
    scraperReady: true,
  },
  // Oregon
  {
    name: "Multnomah County Animal Services",
    city: "Portland",
    state: "OR",
    estimatedDailyIntake: 15,
    platform: "petango",
    scraperReady: true,
  },
  // Illinois
  {
    name: "Chicago Animal Care & Control",
    city: "Chicago",
    state: "IL",
    estimatedDailyIntake: 35,
    platform: "chameleon",
    petharborId: "CHGO",
    scraperReady: true,
  },
];

/** Total estimated daily dog intake across all cataloged shelters */
export const TOTAL_ESTIMATED_DAILY_INTAKE = HIGH_VOLUME_SHELTERS.reduce(
  (sum, s) => sum + s.estimatedDailyIntake,
  0
);

/** Count by platform */
export function countByPlatform(): Record<string, number> {
  const counts: Record<string, number> = {};
  for (const s of HIGH_VOLUME_SHELTERS) {
    counts[s.platform] = (counts[s.platform] || 0) + 1;
  }
  return counts;
}

/** Get shelters that still need scraper work */
export function getUnscrapable(): HighVolumeShelter[] {
  return HIGH_VOLUME_SHELTERS.filter((s) => !s.scraperReady);
}
