import type { UrgencyLevel } from "@/lib/constants";

interface UrgencyBadgeProps {
  level: UrgencyLevel;
  className?: string;
}

const BADGE_CONFIG: Record<
  UrgencyLevel,
  { label: string; className: string }
> = {
  critical: { label: "CRITICAL", className: "urgency-critical" },
  high: { label: "URGENT", className: "urgency-high" },
  medium: { label: "AT RISK", className: "urgency-medium" },
  normal: { label: "AVAILABLE", className: "urgency-normal" },
};

export default function UrgencyBadge({
  level,
  className = "",
}: UrgencyBadgeProps) {
  const config = BADGE_CONFIG[level];

  return (
    <span className={`urgency-badge ${config.className} ${className}`}>
      {level === "critical" && (
        <span className="inline-block w-2 h-2 bg-white rounded-full mr-1" />
      )}
      {config.label}
    </span>
  );
}
