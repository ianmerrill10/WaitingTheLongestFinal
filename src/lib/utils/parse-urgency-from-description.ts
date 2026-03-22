/**
 * Description Urgency Parser
 *
 * Scans dog descriptions for euthanasia/urgency signals and extracts deadline dates.
 * Common patterns in shelter descriptions:
 *   "Will be euthanized by 3/25/2026"
 *   "URGENT - must leave by Friday"
 *   "On the euth list"
 *   "Scheduled for euthanasia on March 28"
 *   "CODE RED - at risk of being put down"
 *   "Rescue only - will be killed if not pulled by 3/30"
 *   "TBD (to be destroyed) 3/26"
 *   "Last day 3/25"
 */

export interface UrgencySignal {
  type: "euthanasia_date" | "at_risk" | "urgent" | "rescue_only";
  date: Date | null;
  confidence: "high" | "medium" | "low";
  matchedText: string;
}

// ── Date extraction helpers (reused from parse-description-date.ts pattern) ──

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

const DAY_MAP: Record<string, number> = {
  sunday: 0, sun: 0,
  monday: 1, mon: 1,
  tuesday: 2, tue: 2, tues: 2,
  wednesday: 3, wed: 3,
  thursday: 4, thu: 4, thur: 4, thurs: 4,
  friday: 5, fri: 5,
  saturday: 6, sat: 6,
};

function parseFutureDate(year: number, month: number, day: number): Date | null {
  if (month < 1 || month > 12) return null;
  if (day < 1 || day > 31) return null;

  const date = new Date(year, month - 1, day);
  if (isNaN(date.getTime())) return null;

  // For urgency dates, we want FUTURE dates (opposite of intake date parsing)
  // But also accept dates within the last 7 days (recently expired deadlines)
  const sevenDaysAgo = new Date();
  sevenDaysAgo.setDate(sevenDaysAgo.getDate() - 7);
  if (date < sevenDaysAgo) return null;

  // Don't accept dates more than 90 days in the future
  const ninetyDaysOut = new Date();
  ninetyDaysOut.setDate(ninetyDaysOut.getDate() + 90);
  if (date > ninetyDaysOut) return null;

  return date;
}

function extractDateAfterPhrase(text: string): Date | null {
  // MM/DD/YYYY or M/D/YYYY
  let m = text.match(/(\d{1,2})\/(\d{1,2})\/(\d{4})/);
  if (m) return parseFutureDate(parseInt(m[3]), parseInt(m[1]), parseInt(m[2]));

  // MM/DD/YY or M/D/YY
  m = text.match(/(\d{1,2})\/(\d{1,2})\/(\d{2})\b/);
  if (m) {
    let year = parseInt(m[3]);
    year += year >= 50 ? 1900 : 2000;
    return parseFutureDate(year, parseInt(m[1]), parseInt(m[2]));
  }

  // MM-DD-YYYY
  m = text.match(/(\d{1,2})-(\d{1,2})-(\d{4})/);
  if (m) return parseFutureDate(parseInt(m[3]), parseInt(m[1]), parseInt(m[2]));

  // "January 5, 2026" or "Jan 5, 2026"
  m = text.match(/([A-Z][a-z]{2,8})\s+(\d{1,2}),?\s+(\d{4})/);
  if (m) {
    const month = MONTH_MAP[m[1].toLowerCase()];
    if (month) return parseFutureDate(parseInt(m[3]), month, parseInt(m[2]));
  }

  // "5 January 2026"
  m = text.match(/(\d{1,2})\s+([A-Z][a-z]{2,8})\s+(\d{4})/);
  if (m) {
    const month = MONTH_MAP[m[2].toLowerCase()];
    if (month) return parseFutureDate(parseInt(m[3]), month, parseInt(m[1]));
  }

  // "March 25" (no year - assume current year, or next year if date passed)
  m = text.match(/([A-Z][a-z]{2,8})\s+(\d{1,2})(?:\s|,|\.|\b)/);
  if (m) {
    const month = MONTH_MAP[m[1].toLowerCase()];
    if (month) {
      const now = new Date();
      let year = now.getFullYear();
      let date = parseFutureDate(year, month, parseInt(m[2]));
      if (!date) {
        date = parseFutureDate(year + 1, month, parseInt(m[2]));
      }
      return date;
    }
  }

  // MM/DD (no year) - assume current/next occurrence
  m = text.match(/(\d{1,2})\/(\d{1,2})(?:\s|,|\.|\b)/);
  if (m) {
    const month = parseInt(m[1]);
    const day = parseInt(m[2]);
    if (month >= 1 && month <= 12 && day >= 1 && day <= 31) {
      const now = new Date();
      let year = now.getFullYear();
      let date = parseFutureDate(year, month, day);
      if (!date) {
        date = parseFutureDate(year + 1, month, day);
      }
      return date;
    }
  }

  // Day of week: "by Friday", "by Monday"
  m = text.match(/\b(sunday|monday|tuesday|wednesday|thursday|friday|saturday|sun|mon|tue|tues|wed|thu|thur|thurs|fri|sat)\b/i);
  if (m) {
    const targetDay = DAY_MAP[m[1].toLowerCase()];
    if (targetDay !== undefined) {
      const now = new Date();
      const currentDay = now.getDay();
      let daysUntil = targetDay - currentDay;
      if (daysUntil <= 0) daysUntil += 7;
      const date = new Date(now);
      date.setDate(date.getDate() + daysUntil);
      date.setHours(17, 0, 0, 0); // Assume 5pm deadline
      return date;
    }
  }

  return null;
}

// ── Urgency pattern matchers ──

// HIGH confidence: explicit euthanasia date mentioned
const EUTHANASIA_DATE_PATTERNS = [
  /(?:euthan(?:ize|ized|asia|ised)|put (?:down|to sleep)|PTS)\s*(?:by|on|date|scheduled|set for|before)?\s*:?\s*/i,
  /(?:scheduled|slated|set|planned)\s+(?:for|to be)\s+(?:euthan(?:ize|ized|asia)|put (?:down|to sleep)|PTS)\s*(?:on|by)?\s*:?\s*/i,
  /(?:will be|to be|going to be|gonna be)\s+(?:euthan(?:ize|ized|asia)|put (?:down|to sleep)|killed|destroyed|PTS)\s*(?:on|by|if not)?\s*:?\s*/i,
  /(?:euth|euthanasia)\s*(?:date|deadline)\s*:?\s*/i,
  /(?:last day|final day|deadline)\s*(?:is|:)?\s*/i,
  /TBD\s*\(?\s*to be destroyed\s*\)?\s*(?:on|by)?\s*:?\s*/i,
  /(?:must leave|must be out|must be pulled|needs? rescue)\s+(?:by|before)\s*/i,
];

// MEDIUM confidence: at-risk / kill list indicators (no date)
const AT_RISK_PATTERNS = [
  /\b(?:CODE RED|code red)\b/i,
  /\b(?:URGENT|EMERGENCY)\s*[-:!]/i,
  /\bon\s+(?:the\s+)?(?:euth(?:anasia)?|kill|death|destroy|at[- ]?risk)\s+list\b/i,
  /\b(?:at[- ]?risk|death row|kill list|destroy list|euth list)\b/i,
  /\b(?:rescue only|rescue needed|needs? rescue|rescue pull)\b/i,
  /\bscheduled for euthanasia\b/i,
  /\bwill be (?:put down|killed|euthanized|destroyed)\b/i,
];

// LOW confidence: general urgency language
const URGENT_PATTERNS = [
  /\bURGENT\b/,
  /\bSAVE ME\b/i,
  /\bNEEDS? (?:OUT|HELP|RESCUE|HOME)\s+(?:NOW|ASAP|IMMEDIATELY|TODAY|URGENT)\b/i,
  /\btime is running out\b/i,
  /\bout of time\b/i,
  /\bno time left\b/i,
  /\bwill die\b/i,
  /\bhelp (?:me|this dog) (?:before|now)\b/i,
  /\blast chance\b/i,
  /\bfinal plea\b/i,
  /\b(?:emergency|critical)\s+(?:foster|placement|adoption)\b/i,
];

export function parseUrgencyFromDescription(
  description: string | null
): UrgencySignal | null {
  if (!description || description.length < 10) return null;

  const text = description;

  // Strategy 1: Look for explicit euthanasia dates (highest value)
  for (const pattern of EUTHANASIA_DATE_PATTERNS) {
    const match = pattern.exec(text);
    if (!match) continue;

    const afterMatch = text.slice(match.index + match[0].length, match.index + match[0].length + 80);
    const date = extractDateAfterPhrase(afterMatch);

    if (date) {
      return {
        type: "euthanasia_date",
        date,
        confidence: "high",
        matchedText: `${match[0]}${afterMatch.slice(0, 30)}`.trim(),
      };
    }

    // Even without a parseable date, the phrase itself is valuable
    return {
      type: "euthanasia_date",
      date: null,
      confidence: "medium",
      matchedText: match[0].trim(),
    };
  }

  // Strategy 2: At-risk / kill list indicators
  for (const pattern of AT_RISK_PATTERNS) {
    const match = pattern.exec(text);
    if (!match) continue;

    // Try to extract a date from nearby context (±100 chars)
    const start = Math.max(0, match.index - 50);
    const end = Math.min(text.length, match.index + match[0].length + 100);
    const context = text.slice(start, end);
    const date = extractDateAfterPhrase(context.slice(context.indexOf(match[0]) + match[0].length));

    return {
      type: "at_risk",
      date,
      confidence: date ? "high" : "medium",
      matchedText: match[0].trim(),
    };
  }

  // Strategy 3: General urgency language
  for (const pattern of URGENT_PATTERNS) {
    const match = pattern.exec(text);
    if (!match) continue;

    return {
      type: "urgent",
      date: null,
      confidence: "low",
      matchedText: match[0].trim(),
    };
  }

  return null;
}
