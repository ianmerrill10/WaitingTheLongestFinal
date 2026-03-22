/**
 * Description Urgency Parser
 *
 * Scans dog descriptions for euthanasia/urgency signals and extracts deadline dates.
 * PRECISION-FIRST: Only flag dogs that are ACTUALLY at risk right now.
 * Past-tense references, backstories, and org boilerplate are filtered out.
 *
 * Common REAL patterns:
 *   "Will be euthanized by 3/25/2026"
 *   "URGENT - must leave by Friday"
 *   "Scheduled for euthanasia on March 28"
 *   "CODE RED - at risk of being put down"
 *   "TBD (to be destroyed) 3/26"
 *   "Last day 3/25"
 */

export interface UrgencySignal {
  type: "euthanasia_date" | "at_risk" | "urgent" | "rescue_only";
  date: Date | null;
  confidence: "high" | "medium" | "low";
  matchedText: string;
}

// ── Date extraction helpers ──

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

  // Accept dates within last 7 days (recently expired) and up to 90 days out
  const sevenDaysAgo = new Date();
  sevenDaysAgo.setDate(sevenDaysAgo.getDate() - 7);
  if (date < sevenDaysAgo) return null;

  const ninetyDaysOut = new Date();
  ninetyDaysOut.setDate(ninetyDaysOut.getDate() + 90);
  if (date > ninetyDaysOut) return null;

  return date;
}

function extractDateAfterPhrase(text: string): Date | null {
  let m;

  // MM/DD/YYYY
  m = text.match(/(\d{1,2})\/(\d{1,2})\/(\d{4})/);
  if (m) return parseFutureDate(parseInt(m[3]), parseInt(m[1]), parseInt(m[2]));

  // MM/DD/YY
  m = text.match(/(\d{1,2})\/(\d{1,2})\/(\d{2})\b/);
  if (m) {
    let year = parseInt(m[3]);
    year += year >= 50 ? 1900 : 2000;
    return parseFutureDate(year, parseInt(m[1]), parseInt(m[2]));
  }

  // MM-DD-YYYY
  m = text.match(/(\d{1,2})-(\d{1,2})-(\d{4})/);
  if (m) return parseFutureDate(parseInt(m[3]), parseInt(m[1]), parseInt(m[2]));

  // "January 5, 2026"
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

  // "March 25" (no year)
  m = text.match(/([A-Z][a-z]{2,8})\s+(\d{1,2})(?:\s|,|\.|\b)/);
  if (m) {
    const month = MONTH_MAP[m[1].toLowerCase()];
    if (month) {
      const now = new Date();
      return parseFutureDate(now.getFullYear(), month, parseInt(m[2]))
        || parseFutureDate(now.getFullYear() + 1, month, parseInt(m[2]));
    }
  }

  // MM/DD (no year)
  m = text.match(/(\d{1,2})\/(\d{1,2})(?:\s|,|\.|\b)/);
  if (m) {
    const month = parseInt(m[1]);
    const day = parseInt(m[2]);
    if (month >= 1 && month <= 12 && day >= 1 && day <= 31) {
      const now = new Date();
      return parseFutureDate(now.getFullYear(), month, day)
        || parseFutureDate(now.getFullYear() + 1, month, day);
    }
  }

  // Day of week: "by Friday"
  m = text.match(/\b(sunday|monday|tuesday|wednesday|thursday|friday|saturday|sun|mon|tue|tues|wed|thu|thur|thurs|fri|sat)\b/i);
  if (m) {
    const targetDay = DAY_MAP[m[1].toLowerCase()];
    if (targetDay !== undefined) {
      const now = new Date();
      let daysUntil = targetDay - now.getDay();
      if (daysUntil <= 0) daysUntil += 7;
      const date = new Date(now);
      date.setDate(date.getDate() + daysUntil);
      date.setHours(17, 0, 0, 0);
      return date;
    }
  }

  return null;
}

// ── False positive filters ──

// Past tense / backstory patterns that should NOT trigger urgency
const PAST_TENSE_NEGATIVES = [
  /(?:was|were|been|got|being)\s+(?:euthanize|euthanised|put down|put to sleep|killed|destroyed)/i,
  /(?:saved|rescued|pulled)\s+(?:from|off)\s+(?:the\s+)?(?:euthanasia|euth|kill|death)\s*(?:list|row)?/i,
  /(?:came|taken|removed|got)\s+(?:from|off)\s+(?:the\s+)?(?:euthanasia|euth|kill|death)\s*(?:list|row)?/i,
  /(?:former|previous|ex)[- ]?(?:death row|euth list|kill list)/i,
  /(?:survived|avoided|escaped|spared)\s+(?:euthanasia|being put down|being killed)/i,
  /(?:no longer|not)\s+(?:on|at)\s+(?:the\s+)?(?:euthanasia|euth|kill|death)\s*(?:list|row)/i,
];

// Org boilerplate — same text across many dogs from same org
const ORG_BOILERPLATE_PATTERNS = [
  /(?:our|we|this)\s+(?:mission|goal|focus|purpose)\s+is\s+(?:to\s+)?(?:rescue|save|pull)\s+(?:dogs|animals|pets)\s+(?:from|off)\s+(?:the\s+)?(?:euthanasia|euth|kill|death)/i,
  /rescuing\s+(?:dogs|animals|pets)\s+from\s+(?:the\s+)?(?:euthanasia|euth|kill|high[- ]?kill)/i,
  /(?:high[- ]?kill|kill)\s+(?:shelters?|facilities?|pounds?)/i,
  /(?:save|rescue)\s+(?:dogs|animals|pets)\s+(?:from|before)\s+(?:euthanasia|being put down|being killed)/i,
];

// ── Urgency pattern matchers ──

// HIGH confidence: explicit future-tense euthanasia deadline for THIS dog
const EUTHANASIA_DATE_PATTERNS = [
  // "will be euthanized/put down/killed by/on [date]" — MUST be future tense
  /(?:will be|is going to be|is scheduled to be|is set to be)\s+(?:euthan(?:ize|ized|asia)|put (?:down|to sleep)|killed|destroyed|PTS)\s*(?:on|by|if not)?\s*:?\s*/i,
  // "scheduled/slated for euthanasia on/by [date]"
  /(?:scheduled|slated|set|planned)\s+(?:for|to be)\s+(?:euthan(?:ize|ized|asia)|put (?:down|to sleep)|PTS)\s*(?:on|by)?\s*:?\s*/i,
  // "euth date: [date]"
  /(?:euth|euthanasia)\s*(?:date|deadline)\s*:?\s*/i,
  // "TBD (to be destroyed) [date]"
  /TBD\s*\(?\s*to be destroyed\s*\)?\s*(?:on|by)?\s*:?\s*/i,
  // "must leave/be out/be pulled by [date]"
  /(?:must leave|must be out|must be pulled)\s+(?:by|before)\s*/i,
  // "last day is [date]" / "final day [date]"
  /(?:last day|final day)\s*(?:is|:)\s*/i,
];

// MEDIUM confidence: at-risk indicators — only match when clearly about THIS dog NOW
const AT_RISK_PATTERNS = [
  /\b(?:CODE RED)\b/,  // All caps CODE RED is very specific
  /\bURGENT\s*[-:!]\s*(?:will be|scheduled|euth|euthanasia|put down)/i,
  /\bon\s+(?:the\s+)?(?:euth(?:anasia)?|kill|death|destroy)\s+list\b/i,
  /\b(?:death row)\b/i,
  /\bscheduled for euthanasia\b/i,
  /\bwill be (?:put down|killed|euthanized|destroyed)\b/i,
];

// LOW confidence: general urgency — only return these if very clearly about this dog
const URGENT_PATTERNS = [
  /\bNEEDS? (?:OUT|RESCUE|HOME)\s+(?:NOW|ASAP|IMMEDIATELY|TODAY|URGENT)\b/i,
  /\btime is running out\b/i,
  /\bout of time\b/i,
  /\bwill die\b/i,
  /\blast chance\b/i,
  /\b(?:emergency|critical)\s+(?:foster|placement|adoption)\b/i,
];

/**
 * Check if the matched text is a false positive (past tense, boilerplate, etc.)
 */
function isFalsePositive(description: string, matchIndex: number): boolean {
  // Get context around the match (200 chars before and after)
  const contextStart = Math.max(0, matchIndex - 200);
  const contextEnd = Math.min(description.length, matchIndex + 200);
  const context = description.slice(contextStart, contextEnd);

  // Check past tense / backstory
  for (const pattern of PAST_TENSE_NEGATIVES) {
    if (pattern.test(context)) return true;
  }

  // Check org boilerplate
  for (const pattern of ORG_BOILERPLATE_PATTERNS) {
    if (pattern.test(context)) return true;
  }

  return false;
}

export function parseUrgencyFromDescription(
  description: string | null
): UrgencySignal | null {
  if (!description || description.length < 10) return null;

  const text = description;

  // Strategy 1: Look for explicit euthanasia dates (highest value)
  for (const pattern of EUTHANASIA_DATE_PATTERNS) {
    const match = pattern.exec(text);
    if (!match) continue;

    // Check for false positives
    if (isFalsePositive(text, match.index)) continue;

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

    // Without a parseable date, only return if the phrase is VERY specific
    // "euth date:", "last day is", "TBD" are specific enough
    // Generic "will be euthanized" without a date is not actionable
    if (/(?:euth|euthanasia)\s*(?:date|deadline)\s*:/i.test(match[0]) ||
        /(?:last day|final day)\s*(?:is|:)/i.test(match[0]) ||
        /TBD/i.test(match[0])) {
      return {
        type: "euthanasia_date",
        date: null,
        confidence: "medium",
        matchedText: match[0].trim(),
      };
    }

    // Skip dateless matches for generic patterns — too many false positives
  }

  // Strategy 2: At-risk / kill list indicators
  for (const pattern of AT_RISK_PATTERNS) {
    const match = pattern.exec(text);
    if (!match) continue;

    // Check for false positives
    if (isFalsePositive(text, match.index)) continue;

    // Try to extract a date from nearby context
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

  // Strategy 3: General urgency language (LOW confidence — only used for "medium" bump)
  for (const pattern of URGENT_PATTERNS) {
    const match = pattern.exec(text);
    if (!match) continue;

    if (isFalsePositive(text, match.index)) continue;

    return {
      type: "urgent",
      date: null,
      confidence: "low",
      matchedText: match[0].trim(),
    };
  }

  return null;
}
