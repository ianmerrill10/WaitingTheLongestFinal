"use client";

import { useEffect, useState } from "react";
import LEDDigit from "./LEDDigit";

interface StatsData {
  current: {
    avg_wait_days: number | null;
    total_dogs: number;
    high_confidence_dogs: number;
    median_wait_days: number | null;
    date_accuracy_pct: number;
    stat_date: string;
  } | null;
  trends: {
    week_change: number | null;
    month_change: number | null;
  } | null;
}

export default function NationalAvgCounter() {
  const [stats, setStats] = useState<StatsData | null>(null);

  useEffect(() => {
    fetch("/api/stats")
      .then(res => res.json())
      .then(setStats)
      .catch(() => {});
  }, []);

  if (!stats?.current || stats.current.avg_wait_days === null) return null;

  const avgDays = stats.current.avg_wait_days;
  const months = Math.floor(avgDays / 30);
  const days = Math.floor(avgDays % 30);
  const hours = Math.floor((avgDays % 1) * 24);

  const values = [
    { val: months, label: "MO" },
    { val: days, label: "DY" },
    { val: hours, label: "HR" },
  ];

  const monthChange = stats.trends?.month_change ?? null;
  const isImproving = monthChange !== null && monthChange < 0;
  const isWorsening = monthChange !== null && monthChange > 0;

  return (
    <div className="bg-white/5 rounded-xl p-6 border border-white/10">
      <div className="text-center">
        <p className="text-gray-300 text-sm mb-1 uppercase tracking-wider font-medium">
          National Average Wait Time
        </p>
        <p className="text-gray-500 text-xs mb-4">
          Across {stats.current.high_confidence_dogs.toLocaleString()} dogs with verified dates
        </p>

        {/* LED Display */}
        <div
          className="led-counter wait-time inline-flex justify-center"
          aria-label={`National average wait time: ${months} months, ${days} days, ${hours} hours`}
        >
          <div className="led-counter-container">
            {values.map((item, i) => {
              const padded = String(item.val).padStart(2, "0");
              return (
                <div key={i} className="flex items-center">
                  <div className="led-digit-group">
                    <div className="led-digits">
                      <LEDDigit value={padded[0]} />
                      <LEDDigit value={padded[1]} />
                    </div>
                    <div className="led-label">{item.label}</div>
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

        {/* Trend */}
        {monthChange !== null && (
          <div className="mt-4 flex items-center justify-center gap-2">
            {isImproving ? (
              <>
                <span className="text-green-400 text-sm font-medium">
                  {Math.abs(monthChange).toFixed(1)} days shorter
                </span>
                <span className="text-gray-500 text-xs">vs 30 days ago</span>
              </>
            ) : isWorsening ? (
              <>
                <span className="text-red-400 text-sm font-medium">
                  +{monthChange.toFixed(1)} days longer
                </span>
                <span className="text-gray-500 text-xs">vs 30 days ago</span>
              </>
            ) : (
              <span className="text-gray-500 text-sm">No change from 30 days ago</span>
            )}
          </div>
        )}
      </div>
    </div>
  );
}
