"use client";

import { SEGMENT_PATTERNS } from "@/lib/constants";

const SEGMENTS = ["a", "b", "c", "d", "e", "f", "g"] as const;

interface LEDDigitProps {
  value: string;
  className?: string;
}

export default function LEDDigit({ value, className = "" }: LEDDigitProps) {
  const pattern = SEGMENT_PATTERNS[value] || SEGMENT_PATTERNS["-"];

  return (
    <div className={`led-digit ${className}`}>
      {SEGMENTS.map((seg, i) => (
        <div
          key={seg}
          className={`led-segment led-segment-${seg} ${pattern[i] ? "active" : ""}`}
        />
      ))}
    </div>
  );
}
