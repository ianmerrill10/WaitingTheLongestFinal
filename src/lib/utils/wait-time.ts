export interface WaitTimeComponents {
  years: number;
  months: number;
  days: number;
  hours: number;
  minutes: number;
  seconds: number;
}

export interface CountdownComponents {
  days: number;
  hours: number;
  minutes: number;
  seconds: number;
  is_critical: boolean;
  is_expired: boolean;
}

/**
 * Calculate wait time components from an intake date.
 * Returns YY:MM:DD:HH:MM:SS breakdown.
 */
export function calculateWaitTime(intakeDate: string | Date): WaitTimeComponents {
  const intake = new Date(intakeDate);
  const now = new Date();
  const diffMs = now.getTime() - intake.getTime();

  if (diffMs < 0) {
    return { years: 0, months: 0, days: 0, hours: 0, minutes: 0, seconds: 0 };
  }

  // Calculate years and months using date arithmetic
  let years = now.getFullYear() - intake.getFullYear();
  let months = now.getMonth() - intake.getMonth();
  let days = now.getDate() - intake.getDate();

  if (days < 0) {
    months--;
    const prevMonth = new Date(now.getFullYear(), now.getMonth(), 0);
    days += prevMonth.getDate();
  }

  if (months < 0) {
    years--;
    months += 12;
  }

  // Compute hours/minutes/seconds from time-of-day difference (avoids negative cascade bug)
  const intakeTimeMs = intake.getHours() * 3600000 + intake.getMinutes() * 60000 + intake.getSeconds() * 1000;
  const nowTimeMs = now.getHours() * 3600000 + now.getMinutes() * 60000 + now.getSeconds() * 1000;
  let timeRemainder = nowTimeMs - intakeTimeMs;
  if (timeRemainder < 0) {
    timeRemainder += 86400000;
    days--;
  }
  if (days < 0) {
    days = 0;
    months--;
    if (months < 0) { months += 12; years--; }
  }

  const h = Math.floor(timeRemainder / 3600000);
  const m = Math.floor((timeRemainder % 3600000) / 60000);
  const s = Math.floor((timeRemainder % 60000) / 1000);

  return {
    years: Math.max(0, years),
    months: Math.max(0, months),
    days: Math.max(0, days),
    hours: h,
    minutes: m,
    seconds: s,
  };
}

/**
 * Calculate countdown components from a euthanasia date.
 * Returns DD:HH:MM:SS breakdown.
 */
export function calculateCountdown(euthanasiaDate: string | Date): CountdownComponents {
  const euthDate = new Date(euthanasiaDate);
  const now = new Date();
  const diffMs = euthDate.getTime() - now.getTime();

  if (diffMs <= 0) {
    return { days: 0, hours: 0, minutes: 0, seconds: 0, is_critical: true, is_expired: true };
  }

  const totalSeconds = Math.floor(diffMs / 1000);
  const days = Math.floor(totalSeconds / 86400);
  const hours = Math.floor((totalSeconds % 86400) / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;

  const totalHours = totalSeconds / 3600;
  const is_critical = totalHours < 24;

  return { days, hours, minutes, seconds, is_critical, is_expired: false };
}

/**
 * Format wait time components as YY:MM:DD:HH:MM:SS string.
 */
export function formatWaitTime(wt: WaitTimeComponents): string {
  const pad = (n: number) => String(n).padStart(2, "0");
  return `${pad(wt.years)}:${pad(wt.months)}:${pad(wt.days)}:${pad(wt.hours)}:${pad(wt.minutes)}:${pad(wt.seconds)}`;
}

/**
 * Format countdown components as DD:HH:MM:SS string.
 */
export function formatCountdown(cd: CountdownComponents): string {
  const pad = (n: number) => String(n).padStart(2, "0");
  return `${pad(cd.days)}:${pad(cd.hours)}:${pad(cd.minutes)}:${pad(cd.seconds)}`;
}

/**
 * Get total days waiting from intake date.
 */
export function getTotalDaysWaiting(intakeDate: string | Date): number {
  const intake = new Date(intakeDate);
  const now = new Date();
  return Math.floor((now.getTime() - intake.getTime()) / 86400000);
}

// incrementWaitTime() and decrementCountdown() intentionally removed.
// Both used approximate arithmetic (hardcoded 30-day months, simple ±1s carry)
// that accumulated drift over time and produced wrong calendar values at
// month boundaries.
//
// Components must call calculateWaitTime(intakeDate) or
// calculateCountdown(euthanasiaDate) on EVERY interval tick instead.
// Re-deriving from the source timestamp each second makes drift impossible.
