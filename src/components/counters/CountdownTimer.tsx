"use client";

import { useEffect, useState, useCallback } from "react";
import LEDDigit from "./LEDDigit";
import {
  calculateCountdown,
  type CountdownComponents,
} from "@/lib/utils/wait-time";
import { COUNTDOWN_LABELS } from "@/lib/constants";

interface CountdownTimerProps {
  euthanasiaDate: string;
  compact?: boolean;
  className?: string;
  urgencyLevel?: string;
}

export default function CountdownTimer({
  euthanasiaDate,
  compact = false,
  className = "",
  urgencyLevel,
}: CountdownTimerProps) {
  const [countdown, setCountdown] = useState<CountdownComponents | null>(null);

  // Re-derive from the source timestamp on every tick.
  // This is the only drift-free approach: no accumulated carry errors,
  // no approximate month lengths, no floating-point accumulation.
  const tick = useCallback(() => {
    setCountdown(calculateCountdown(euthanasiaDate));
  }, [euthanasiaDate]);

  useEffect(() => {
    tick(); // Hydrate immediately on mount
    const interval = setInterval(tick, 1000);
    return () => clearInterval(interval);
  }, [tick]);

  if (!countdown) return null;

  if (countdown.is_expired) {
    return (
      <div className={`text-red-600 font-bold text-sm ${className}`}>
        TIME EXPIRED
      </div>
    );
  }

  const values = [
    countdown.days,
    countdown.hours,
    countdown.minutes,
    countdown.seconds,
  ];

  const sizeClass = compact ? "led-counter-sm" : "";
  const criticalClass = countdown.is_critical ? "critical" : "";
  const urgencyClass = urgencyLevel === "high" || urgencyLevel === "medium" ? "urgent-high" : "";

  return (
    <div
      className={`led-counter countdown ${criticalClass} ${urgencyClass} ${sizeClass} ${className}`}
      aria-label={`${countdown.days} days, ${countdown.hours} hours remaining`}
    >
      <div className="led-counter-container">
        {values.map((val, i) => {
          const padded = String(val).padStart(2, "0");
          return (
            <div key={i} className="flex items-center">
              <div className="led-digit-group">
                <div className="led-digits">
                  <LEDDigit value={padded[0]} />
                  <LEDDigit value={padded[1]} />
                </div>
                <div className="led-label">{COUNTDOWN_LABELS[i]}</div>
              </div>
              {i < values.length - 1 && (
                <div className="led-separator">
                  <div className="led-separator-dot" />
                  <div className="led-separator-dot" />
                </div>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
}
