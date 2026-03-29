import type { Metadata } from "next";
import Link from "next/link";
import { notFound } from "next/navigation";
import { createAdminClient } from "@/lib/supabase/admin";
import {
  computeCredibilityScore,
  getCredibilityLabel,
} from "@/lib/utils/credibility-score";
import type { SourceLink } from "@/types/dog";

export async function generateMetadata({
  params,
}: {
  params: Promise<{ id: string }>;
}): Promise<Metadata> {
  const { id } = await params;
  try {
    const supabase = createAdminClient();
    const { data: dog } = await supabase
      .from("dogs")
      .select("name, breed_primary")
      .eq("id", id)
      .single();
    return {
      title: `Data Sources - ${dog?.name || "Dog"} | WaitingTheLongest.com`,
      description: `View the verified data sources, credibility score, and audit trail for ${dog?.name || "this dog"}.`,
    };
  } catch {
    return { title: "Data Sources" };
  }
}

export default async function DogSourcesPage({
  params,
}: {
  params: Promise<{ id: string }>;
}) {
  const { id } = await params;
  const supabase = createAdminClient();

  const { data: dog, error } = await supabase
    .from("dogs")
    .select(
      `
      id, name, breed_primary, primary_photo_url,
      external_id, external_source, external_url, external_url_alive,
      source_links, original_intake_date, source_extraction_method,
      credibility_score, source_nonprofit_verified,
      date_confidence, date_source, intake_date,
      verification_status, last_verified_at, last_synced_at,
      dedup_merged_from, created_at, updated_at,
      is_foster, transfer_origin_shelter_id, transfer_original_intake,
      shelters (
        id, name, city, state_code, website, phone, email,
        external_id, external_source, is_verified
      )
    `
    )
    .eq("id", id)
    .single();

  if (error || !dog) notFound();

  // Fetch transfer origin shelter if applicable
  let transferShelter: { id: string; name: string; city: string; state_code: string } | null = null;
  if (dog.transfer_origin_shelter_id) {
    const { data: ts } = await supabase
      .from("shelters")
      .select("id, name, city, state_code")
      .eq("id", dog.transfer_origin_shelter_id)
      .single();
    transferShelter = ts;
  }

  // Fetch audit logs for this dog
  const { data: auditLogs } = await supabase
    .from("audit_logs")
    .select("audit_type, severity, message, action_taken, details, created_at")
    .eq("entity_id", id)
    .eq("entity_type", "dog")
    .order("created_at", { ascending: false })
    .limit(20);

  const shelter = Array.isArray(dog.shelters) ? dog.shelters[0] : dog.shelters;
  const sourceLinks: SourceLink[] = dog.source_links || [];
  const breakdown = computeCredibilityScore({
    source_links: sourceLinks,
    date_confidence: dog.date_confidence,
    verification_status: dog.verification_status,
    last_verified_at: dog.last_verified_at,
    source_nonprofit_verified: dog.source_nonprofit_verified,
    external_source: dog.external_source,
  });
  const credLabel = getCredibilityLabel(breakdown.total);

  const sourceDisplayName = dog.external_source === "rescuegroups"
    ? "RescueGroups.org API v5"
    : dog.external_source?.startsWith("scraper_")
      ? `Web Scraper (${dog.external_source.replace("scraper_", "").replace(/_/g, " ")})`
      : dog.external_source || "Unknown";

  const extractionMethod = dog.source_extraction_method || "Unknown";

  return (
    <div className="min-h-screen bg-gray-50">
      {/* Header */}
      <div className="bg-white border-b">
        <div className="max-w-4xl mx-auto px-4 py-6">
          <Link
            href={`/dogs/${dog.id}`}
            className="text-sm text-blue-600 hover:underline mb-2 inline-block"
          >
            &larr; Back to {dog.name}&apos;s profile
          </Link>
          <h1 className="text-2xl font-bold text-gray-900">
            Data Sources &amp; Verification
          </h1>
          <p className="text-gray-600 mt-1">
            {dog.name} &mdash; {dog.breed_primary}
          </p>
        </div>
      </div>

      <div className="max-w-4xl mx-auto px-4 py-8 space-y-8">
        {/* Credibility Score */}
        <section className="bg-white rounded-xl border p-6">
          <h2 className="text-lg font-semibold text-gray-900 mb-4">
            Credibility Score
          </h2>
          <div className="flex items-center gap-4 mb-6">
            <div
              className={`text-4xl font-bold ${credLabel.color}`}
            >
              {breakdown.total}
            </div>
            <div>
              <span
                className={`inline-block px-3 py-1 rounded-full text-sm font-semibold ${credLabel.color} ${credLabel.bgColor}`}
              >
                {credLabel.label}
              </span>
              <p className="text-sm text-gray-500 mt-1">out of 100 points</p>
            </div>
          </div>

          <div className="w-full bg-gray-200 rounded-full h-3 mb-6">
            <div
              className={`h-3 rounded-full transition-all ${
                breakdown.total >= 80
                  ? "bg-green-500"
                  : breakdown.total >= 60
                    ? "bg-blue-500"
                    : breakdown.total >= 40
                      ? "bg-yellow-500"
                      : "bg-red-500"
              }`}
              style={{ width: `${breakdown.total}%` }}
            />
          </div>

          <table className="w-full text-sm">
            <thead>
              <tr className="text-left text-gray-500 border-b">
                <th className="pb-2 font-medium">Factor</th>
                <th className="pb-2 font-medium text-right">Points</th>
                <th className="pb-2 font-medium text-right">Max</th>
              </tr>
            </thead>
            <tbody className="divide-y">
              <ScoreRow label="Verifiable Source Links" pts={breakdown.sourceLinks} max={25} />
              <ScoreRow label="Date Confidence" pts={breakdown.dateConfidence} max={20} />
              <ScoreRow label="Verification Status" pts={breakdown.verificationStatus} max={20} />
              <ScoreRow label="Verification Recency" pts={breakdown.verificationRecency} max={15} />
              <ScoreRow label="Shelter Nonprofit Verified" pts={breakdown.shelterVerified} max={10} />
              <ScoreRow label="Data Source Quality" pts={breakdown.sourceType} max={10} />
            </tbody>
          </table>
        </section>

        {/* Primary Data Source */}
        <section className="bg-white rounded-xl border p-6">
          <h2 className="text-lg font-semibold text-gray-900 mb-4">
            Primary Data Source
          </h2>
          <div className="space-y-3">
            <InfoRow label="Source" value={sourceDisplayName} />
            <InfoRow label="Extraction Method" value={extractionMethod} />
            <InfoRow label="External ID" value={dog.external_id || "N/A"} />
            <InfoRow
              label="Last Synced"
              value={dog.last_synced_at ? new Date(dog.last_synced_at).toLocaleString() : "N/A"}
            />
            <InfoRow
              label="First Imported"
              value={new Date(dog.created_at).toLocaleString()}
            />
            <InfoRow
              label="Last Updated"
              value={new Date(dog.updated_at).toLocaleString()}
            />
          </div>
        </section>

        {/* Verifiable Links */}
        <section className="bg-white rounded-xl border p-6">
          <h2 className="text-lg font-semibold text-gray-900 mb-4">
            Verifiable Links ({sourceLinks.length})
          </h2>
          {sourceLinks.length === 0 ? (
            <p className="text-gray-500 text-sm">
              No source links recorded yet. Links are added during data sync and
              verification cycles.
            </p>
          ) : (
            <div className="space-y-3">
              {sourceLinks.map((link, i) => (
                <div key={i} className="flex items-start gap-3 p-3 bg-gray-50 rounded-lg">
                  <StatusDot code={link.status_code} />
                  <div className="flex-1 min-w-0">
                    <a
                      href={link.url}
                      target="_blank"
                      rel="noopener noreferrer"
                      className="text-blue-600 hover:underline text-sm break-all"
                    >
                      {link.url}
                    </a>
                    <p className="text-xs text-gray-500 mt-1">
                      {link.description} &mdash; via{" "}
                      {link.source.replace(/_/g, " ")}
                    </p>
                    <p className="text-xs text-gray-400">
                      Last checked:{" "}
                      {link.checked_at
                        ? new Date(link.checked_at).toLocaleString()
                        : "Never"}
                      {link.status_code !== null &&
                        ` (HTTP ${link.status_code})`}
                    </p>
                  </div>
                </div>
              ))}
            </div>
          )}
          {dog.external_url && !sourceLinks.some((l) => l.url === dog.external_url) && (
            <div className="mt-4 p-3 bg-blue-50 rounded-lg">
              <p className="text-sm text-blue-800">
                Original listing:{" "}
                <a
                  href={dog.external_url}
                  target="_blank"
                  rel="noopener noreferrer"
                  className="underline"
                >
                  {dog.external_url}
                </a>
                {dog.external_url_alive === false && (
                  <span className="ml-2 text-red-600 font-medium">(Link Dead)</span>
                )}
              </p>
            </div>
          )}
        </section>

        {/* Shelter Verification */}
        <section className="bg-white rounded-xl border p-6">
          <h2 className="text-lg font-semibold text-gray-900 mb-4">
            Shelter Verification
          </h2>
          <div className="space-y-3">
            <InfoRow label="Shelter" value={shelter?.name || "Unknown"} />
            <InfoRow
              label="Location"
              value={
                shelter
                  ? `${shelter.city}, ${shelter.state_code}`
                  : "Unknown"
              }
            />
            <InfoRow
              label="Nonprofit Verified"
              value={
                dog.source_nonprofit_verified
                  ? "Yes — verified via EIN/RescueGroups"
                  : "Not yet verified"
              }
            />
            <InfoRow
              label="Shelter Source"
              value={shelter?.external_source || "Unknown"}
            />
            {shelter?.website && (
              <InfoRow
                label="Website"
                value={
                  <a
                    href={shelter.website}
                    target="_blank"
                    rel="noopener noreferrer"
                    className="text-blue-600 hover:underline"
                  >
                    {shelter.website}
                  </a>
                }
              />
            )}
            {shelter?.id && (
              <div className="pt-2">
                <Link
                  href={`/shelters/${shelter.id}`}
                  className="text-blue-600 hover:underline text-sm"
                >
                  View full shelter profile &rarr;
                </Link>
              </div>
            )}
          </div>
        </section>

        {/* Date Provenance */}
        <section className="bg-white rounded-xl border p-6">
          <h2 className="text-lg font-semibold text-gray-900 mb-4">
            Date Provenance
          </h2>
          <div className="space-y-3">
            <InfoRow
              label="Current Intake Date"
              value={
                dog.intake_date
                  ? new Date(dog.intake_date).toLocaleDateString()
                  : "Unknown"
              }
            />
            <InfoRow
              label="Original Intake Date"
              value={
                dog.original_intake_date
                  ? new Date(dog.original_intake_date).toLocaleDateString()
                  : "Same as current"
              }
            />
            {dog.original_intake_date &&
              dog.intake_date &&
              dog.original_intake_date !== dog.intake_date && (
                <div className="p-3 bg-yellow-50 rounded-lg">
                  <p className="text-sm text-yellow-800">
                    Date was adjusted from original. Reason: date capping or
                    audit correction applied for accuracy.
                  </p>
                </div>
              )}
            <InfoRow
              label="Date Confidence"
              value={
                <span className="capitalize font-medium">
                  {dog.date_confidence || "Unknown"}
                </span>
              }
            />
            <InfoRow
              label="Date Source"
              value={formatDateSource(dog.date_source)}
            />
          </div>
        </section>

        {/* Transfer History */}
        {transferShelter && dog.transfer_original_intake && (
          <section className="bg-white rounded-xl border p-6">
            <h2 className="text-lg font-semibold text-gray-900 mb-4">
              Transfer History
            </h2>
            <div className="space-y-4">
              <div className="flex items-start gap-3">
                <div className="w-3 h-3 rounded-full bg-blue-500 mt-1.5 flex-shrink-0" />
                <div>
                  <p className="text-sm font-medium text-gray-900">
                    Originally at{" "}
                    <Link href={`/shelters/${transferShelter.id}`} className="text-blue-600 hover:underline">
                      {transferShelter.name}
                    </Link>
                  </p>
                  <p className="text-xs text-gray-500">
                    {transferShelter.city}, {transferShelter.state_code} &mdash; intake{" "}
                    {new Date(dog.transfer_original_intake).toLocaleDateString()}
                  </p>
                </div>
              </div>
              <div className="flex items-start gap-3">
                <div className="w-3 h-3 rounded-full bg-green-500 mt-1.5 flex-shrink-0" />
                <div>
                  <p className="text-sm font-medium text-gray-900">
                    Transferred to{" "}
                    <Link href={`/shelters/${shelter?.id}`} className="text-blue-600 hover:underline">
                      {shelter?.name || "Current shelter"}
                    </Link>
                  </p>
                  <p className="text-xs text-gray-500">
                    Cumulative wait:{" "}
                    <span className="font-semibold text-amber-600">
                      {Math.floor((Date.now() - new Date(dog.transfer_original_intake).getTime()) / 86400000)} days
                    </span>
                  </p>
                </div>
              </div>
            </div>
          </section>
        )}

        {/* Foster Status */}
        {dog.is_foster && (
          <section className="bg-teal-50 rounded-xl border border-teal-200 p-6">
            <h2 className="text-lg font-semibold text-teal-900 mb-2">
              Foster Placement
            </h2>
            <p className="text-sm text-teal-700">
              This dog is currently in a foster home rather than at the shelter facility.
              Foster homes provide a more comfortable environment while the dog awaits adoption.
              Contact the shelter to arrange a meet-and-greet.
            </p>
          </section>
        )}

        {/* Verification History */}
        <section className="bg-white rounded-xl border p-6">
          <h2 className="text-lg font-semibold text-gray-900 mb-4">
            Verification Status
          </h2>
          <div className="space-y-3">
            <InfoRow
              label="Status"
              value={
                <span
                  className={`inline-block px-2 py-0.5 rounded text-xs font-semibold ${
                    dog.verification_status === "verified"
                      ? "bg-green-100 text-green-800"
                      : dog.verification_status === "not_found"
                        ? "bg-red-100 text-red-800"
                        : "bg-gray-100 text-gray-800"
                  }`}
                >
                  {dog.verification_status || "Unverified"}
                </span>
              }
            />
            <InfoRow
              label="Last Verified"
              value={
                dog.last_verified_at
                  ? new Date(dog.last_verified_at).toLocaleString()
                  : "Never verified"
              }
            />
          </div>
        </section>

        {/* Merge History */}
        {dog.dedup_merged_from &&
          Array.isArray(dog.dedup_merged_from) &&
          dog.dedup_merged_from.length > 0 && (
            <section className="bg-white rounded-xl border p-6">
              <h2 className="text-lg font-semibold text-gray-900 mb-4">
                Merged Data Sources
              </h2>
              <p className="text-sm text-gray-600 mb-3">
                This listing was created by merging data from multiple sources to
                provide the most complete information.
              </p>
              <div className="space-y-2">
                {dog.dedup_merged_from.map((src: string, i: number) => (
                  <div key={i} className="p-2 bg-gray-50 rounded text-sm text-gray-700">
                    {String(src).replace(/_/g, " ")}
                  </div>
                ))}
              </div>
            </section>
          )}

        {/* Audit Trail */}
        {auditLogs && auditLogs.length > 0 && (
          <section className="bg-white rounded-xl border p-6">
            <h2 className="text-lg font-semibold text-gray-900 mb-4">
              Audit Trail ({auditLogs.length} entries)
            </h2>
            <div className="space-y-3">
              {auditLogs.map((log, i) => (
                <div key={i} className="p-3 bg-gray-50 rounded-lg border-l-4 border-gray-300">
                  <div className="flex items-center gap-2 mb-1">
                    <span
                      className={`text-xs font-semibold px-2 py-0.5 rounded ${
                        log.severity === "critical"
                          ? "bg-red-100 text-red-700"
                          : log.severity === "error"
                            ? "bg-orange-100 text-orange-700"
                            : "bg-blue-100 text-blue-700"
                      }`}
                    >
                      {log.severity}
                    </span>
                    <span className="text-xs text-gray-500">
                      {log.audit_type}
                    </span>
                    <span className="text-xs text-gray-400 ml-auto">
                      {new Date(log.created_at).toLocaleString()}
                    </span>
                  </div>
                  <p className="text-sm text-gray-700">{log.message}</p>
                  {log.action_taken && (
                    <p className="text-xs text-gray-500 mt-1">
                      Action: {log.action_taken}
                    </p>
                  )}
                </div>
              ))}
            </div>
          </section>
        )}

        {/* Legal Disclaimer */}
        <section className="bg-gray-100 rounded-xl p-6">
          <h2 className="text-sm font-semibold text-gray-700 mb-2">
            Legal Notice
          </h2>
          <p className="text-xs text-gray-500 leading-relaxed">
            This data provenance page documents the sources used to compile this
            dog listing. WaitingTheLongest.com aggregates information from
            verified shelter databases, public shelter websites, and automated
            monitoring systems. We make reasonable efforts to verify data
            accuracy but cannot guarantee that any information displayed is
            current, complete, or error-free. Euthanasia dates and countdown
            timers are informational estimates only. Always contact the shelter
            directly to verify a dog&apos;s current status before taking any
            action. See our{" "}
            <Link
              href="/legal/terms-of-service"
              className="text-blue-600 hover:underline"
            >
              Terms of Service
            </Link>{" "}
            for full disclaimers.
          </p>
        </section>
      </div>
    </div>
  );
}

function ScoreRow({ label, pts, max }: { label: string; pts: number; max: number }) {
  return (
    <tr>
      <td className="py-2 text-gray-700">{label}</td>
      <td className="py-2 text-right font-mono font-semibold text-gray-900">
        {pts}
      </td>
      <td className="py-2 text-right text-gray-400">/ {max}</td>
    </tr>
  );
}

function InfoRow({
  label,
  value,
}: {
  label: string;
  value: React.ReactNode;
}) {
  return (
    <div className="flex flex-col sm:flex-row sm:items-start gap-1 sm:gap-4">
      <span className="text-sm text-gray-500 sm:w-48 flex-shrink-0">
        {label}
      </span>
      <span className="text-sm text-gray-900 break-all">{value}</span>
    </div>
  );
}

function StatusDot({ code }: { code: number | null }) {
  if (code === null) {
    return (
      <span className="w-3 h-3 rounded-full bg-gray-300 flex-shrink-0 mt-1" title="Not checked" />
    );
  }
  if (code >= 200 && code < 400) {
    return (
      <span className="w-3 h-3 rounded-full bg-green-500 flex-shrink-0 mt-1" title={`HTTP ${code}`} />
    );
  }
  return (
    <span className="w-3 h-3 rounded-full bg-red-500 flex-shrink-0 mt-1" title={`HTTP ${code}`} />
  );
}

function formatDateSource(source: string | null): string {
  if (!source) return "Unknown";
  const parts = source.split("|");
  const labels: Record<string, string> = {
    rescuegroups_available_date: "RescueGroups available date (shelter-provided)",
    rescuegroups_found_date: "RescueGroups found date (shelter-provided)",
    rescuegroups_created_date: "RescueGroups listing creation date",
    rescuegroups_created_date_active: "RescueGroups listing creation date (active listing)",
    rescuegroups_updated_date: "RescueGroups last update date",
    no_date_available: "No date available from source",
    description_parsed: "Parsed from listing description text",
  };
  return parts
    .map((p) => {
      const trimmed = p.trim();
      if (trimmed.startsWith("description_parsed:"))
        return `Parsed from description: "${trimmed.split(":").slice(1).join(":").trim()}"`;
      if (trimmed.startsWith("stray_hold_adjusted_"))
        return `Adjusted for stray hold period (${trimmed.replace("stray_hold_adjusted_", "")})`;
      if (trimmed.startsWith("wait_capped_"))
        return `Wait time capped at ${trimmed.replace("wait_capped_", "")}`;
      if (trimmed.startsWith("age_capped"))
        return "Capped because wait exceeded dog's estimated age";
      return labels[trimmed] || trimmed.replace(/_/g, " ");
    })
    .join(" → ");
}
