/**
 * Extract the earliest meaningful date from a dog's description text.
 * Many RescueGroups listings contain phrases like:
 *   "Posted 2/18/18"
 *   "Listed on 01/15/2020"
 *   "Found on 1/19/18"
 *   "In rescue since March 2019"
 *   "Available since 12/2020"
 *   "Intake date: 3/15/2022"
 *   "Surrendered on June 5, 2021"
 *   "pictures updated 5/10/20"
 *
 * Returns the parsed date if found, or null.
 */

// Patterns that indicate an intake/listing date (ordered by reliability)
const DATE_CONTEXT_PATTERNS = [
  // "Posted 2/18/18", "posted on 2/18/18"
  /(?:posted|listed|available|surrendered|found|rescued|intake|received|entered|arrived|brought in|came in|taken in)\s*(?:on|since|date)?:?\s*/i,
  // "in rescue since March 2019"
  /(?:in (?:rescue|shelter|foster|care|our care|the shelter)\s*since)\s*/i,
  // "intake date: ..."
  /(?:intake|arrival|admit|entry)\s*date\s*:?\s*/i,
];

// Date formats to match after the context phrase
const DATE_PATTERNS: { regex: RegExp; parse: (m: RegExpMatchArray) => Date | null }[] = [
  // MM/DD/YYYY or M/D/YYYY
  {
    regex: /(\d{1,2})\/(\d{1,2})\/(\d{4})/,
    parse: (m) => parseDate(parseInt(m[3]), parseInt(m[1]), parseInt(m[2])),
  },
  // MM/DD/YY or M/D/YY
  {
    regex: /(\d{1,2})\/(\d{1,2})\/(\d{2})\b/,
    parse: (m) => {
      let year = parseInt(m[3]);
      year += year >= 50 ? 1900 : 2000;
      return parseDate(year, parseInt(m[1]), parseInt(m[2]));
    },
  },
  // MM-DD-YYYY
  {
    regex: /(\d{1,2})-(\d{1,2})-(\d{4})/,
    parse: (m) => parseDate(parseInt(m[3]), parseInt(m[1]), parseInt(m[2])),
  },
  // "January 5, 2021" or "Jan 5, 2021"
  {
    regex: /([A-Z][a-z]{2,8})\s+(\d{1,2}),?\s+(\d{4})/,
    parse: (m) => {
      const month = MONTH_MAP[m[1].toLowerCase()];
      if (!month) return null;
      return parseDate(parseInt(m[3]), month, parseInt(m[2]));
    },
  },
  // "5 January 2021"
  {
    regex: /(\d{1,2})\s+([A-Z][a-z]{2,8})\s+(\d{4})/,
    parse: (m) => {
      const month = MONTH_MAP[m[2].toLowerCase()];
      if (!month) return null;
      return parseDate(parseInt(m[3]), month, parseInt(m[1]));
    },
  },
  // "March 2019" (no day — use 1st)
  {
    regex: /([A-Z][a-z]{2,8})\s+(\d{4})/,
    parse: (m) => {
      const month = MONTH_MAP[m[1].toLowerCase()];
      if (!month) return null;
      return parseDate(parseInt(m[2]), month, 1);
    },
  },
  // MM/YYYY
  {
    regex: /(\d{1,2})\/(\d{4})/,
    parse: (m) => parseDate(parseInt(m[2]), parseInt(m[1]), 1),
  },
];

const MONTH_MAP: Record<string, number> = {
  january: 1, jan: 1,
  february: 2, feb: 2,
  march: 3, mar: 3,
  april: 4, apr: 4,
  may: 5,
  june: 6, jun: 6,
  july: 7, jul: 7,
  august: 8, aug: 8,
  september: 9, sep: 9, sept: 9,
  october: 10, oct: 10,
  november: 11, nov: 11,
  december: 12, dec: 12,
};

function parseDate(year: number, month: number, day: number): Date | null {
  if (year < 2000 || year > 2030) return null;
  if (month < 1 || month > 12) return null;
  if (day < 1 || day > 31) return null;

  const date = new Date(year, month - 1, day);
  if (isNaN(date.getTime())) return null;

  // Don't return future dates
  if (date > new Date()) return null;

  return date;
}

export interface ParsedDateResult {
  date: Date;
  source: string; // The matched text for debugging
  confidence: "high" | "medium";
}

export function parseDateFromDescription(description: string | null): ParsedDateResult | null {
  if (!description) return null;

  // Strategy 1: Look for dates preceded by context phrases (highest confidence)
  for (const contextPattern of DATE_CONTEXT_PATTERNS) {
    const contextMatch = contextPattern.exec(description);
    if (!contextMatch) continue;

    // Search for a date pattern after the context phrase
    const afterContext = description.slice(contextMatch.index + contextMatch[0].length);
    // Only look at the next ~50 chars after the context phrase
    const searchWindow = afterContext.slice(0, 50);

    for (const { regex, parse } of DATE_PATTERNS) {
      const dateMatch = regex.exec(searchWindow);
      if (dateMatch) {
        const date = parse(dateMatch);
        if (date) {
          return {
            date,
            source: `${contextMatch[0]}${dateMatch[0]}`.trim(),
            confidence: "high",
          };
        }
      }
    }
  }

  // Strategy 2: Look for standalone "Posted M/D/YY" pattern (very common in RG)
  const postedPattern = /(?:^|\.\s*|\n)(?:Posted|Listed)\s+(\d{1,2}\/\d{1,2}\/\d{2,4})/i;
  const postedMatch = postedPattern.exec(description);
  if (postedMatch) {
    const dateStr = postedMatch[1];
    for (const { regex, parse } of DATE_PATTERNS) {
      const m = regex.exec(dateStr);
      if (m) {
        const date = parse(m);
        if (date) {
          return { date, source: postedMatch[0].trim(), confidence: "high" };
        }
      }
    }
  }

  return null;
}
