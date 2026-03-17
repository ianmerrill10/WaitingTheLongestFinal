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

  const hours = now.getHours() - intake.getHours();
  const minutes = now.getMinutes() - intake.getMinutes();
  const seconds = now.getSeconds() - intake.getSeconds();

  // Normalize time components
  let h = hours;
  let m = minutes;
  let s = seconds;

  if (s < 0) { s += 60; m--; }
  if (m < 0) { m += 60; h--; }
  if (h < 0) { h += 24; days--; }
  if (days < 0) { days = 0; }

  return {
    years: Math.max(0, years),
    months: Math.max(0, months),
    days: Math.max(0, days),
    hours: Math.max(0, h),
    minutes: Math.max(0, m),
    seconds: Math.max(0, s),
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

/**
 * Increment wait time by one second (client-side ticking).
 */
export function incrementWaitTime(wt: WaitTimeComponents): WaitTimeComponents {
  let { years, months, days, hours, minutes, seconds } = wt;
  seconds++;
  if (seconds >= 60) { seconds = 0; minutes++; }
  if (minutes >= 60) { minutes = 0; hours++; }
  if (hours >= 24) { hours = 0; days++; }
  // Days overflow into months is approximate (30 days)
  if (days >= 30) { days = 0; months++; }
  if (months >= 12) { months = 0; years++; }
  return { years, months, days, hours, minutes, seconds };
}

/**
 * Decrement countdown by one second (client-side ticking).
 */
export function decrementCountdown(cd: CountdownComponents): CountdownComponents {
  if (cd.is_expired) return cd;

  let { days, hours, minutes, seconds } = cd;
  seconds--;
  if (seconds < 0) { seconds = 59; minutes--; }
  if (minutes < 0) { minutes = 59; hours--; }
  if (hours < 0) { hours = 23; days--; }

  const is_expired = days < 0;
  if (is_expired) {
    return { days: 0, hours: 0, minutes: 0, seconds: 0, is_critical: true, is_expired: true };
  }

  const totalHours = days * 24 + hours;
  return { days, hours, minutes, seconds, is_critical: totalHours < 24, is_expired: false };
}
