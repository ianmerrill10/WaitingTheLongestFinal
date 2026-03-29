"use client";

import { useEffect, useState, useCallback } from "react";
import LEDDigit from "./LEDDigit";
import {
  calculateWaitTime,
  incrementWaitTime,
  type WaitTimeComponents,
} from "@/lib/utils/wait-time";
import { WAIT_TIME_LABELS } from "@/lib/constants";

interface LEDCounterProps {
  intakeDate: string;
  compact?: boolean;
  className?: string;
}

export default function LEDCounter({
  intakeDate,
  compact = false,
  className = "",
}: LEDCounterProps) {
  const [time, setTime] = useState<WaitTimeComponents | null>(null);

  useEffect(() => {
    setTime(calculateWaitTime(intakeDate));
  }, [intakeDate]);

  const tick = useCallback(() => {
    setTime((prev) => (prev ? incrementWaitTime(prev) : null));
  }, []);

  useEffect(() => {
    const interval = setInterval(tick, 1000);
    return () => clearInterval(interval);
  }, [tick]);

  if (!time) return null;

  const values = [
    Math.min(time.years, 99),
    time.months,
    time.days,
    time.hours,
    time.minutes,
    time.seconds,
  ];

  const sizeClass = compact ? "led-counter-sm" : "";

  return (
    <div
      className={`led-counter wait-time ${sizeClass} ${className}`}
      aria-label={`Waiting ${time.years} years, ${time.months} months, ${time.days} days`}
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
                <div className="led-label">{WAIT_TIME_LABELS[i]}</div>
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
