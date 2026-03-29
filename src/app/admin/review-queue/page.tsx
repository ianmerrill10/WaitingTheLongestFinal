import Link from "next/link";
import { createAdminClient } from "@/lib/supabase/admin";

export const dynamic = "force-dynamic";

export default async function ReviewQueuePage() {
  const supabase = createAdminClient();

  // Fetch open review items with dog info
  const { data: items } = await supabase
    .from("review_queue")
    .select(`
      id, reason, priority, state, notes, created_at,
      dogs (
        id, name, breed_primary, intake_date, date_confidence,
        date_source, primary_photo_url, is_available,
        shelters (name, city, state_code)
      )
    `)
    .eq("state", "open")
    .order("priority", { ascending: false })
    .order("created_at", { ascending: true })
    .limit(100);

  // Stats
  const { count: openCount } = await supabase
    .from("review_queue")
    .select("id", { count: "exact", head: true })
    .eq("state", "open");

  const { count: resolvedCount } = await supabase
    .from("review_queue")
    .select("id", { count: "exact", head: true })
    .eq("state", "resolved");

  const reasonLabels: Record<string, string> = {
    missing_intake_date: "Missing Intake Date",
    conflicting_intake_dates: "Conflicting Dates",
    possible_duplicate: "Possible Duplicate",
    status_unclear: "Status Unclear",
    transfer_suspected: "Transfer Suspected",
    suspiciously_old_listing: "Suspiciously Old",
    date_confidence_low: "Low Confidence Date",
  };

  const reasonColors: Record<string, string> = {
    missing_intake_date: "bg-red-100 text-red-700",
    conflicting_intake_dates: "bg-orange-100 text-orange-700",
    possible_duplicate: "bg-purple-100 text-purple-700",
    status_unclear: "bg-gray-100 text-gray-700",
    transfer_suspected: "bg-blue-100 text-blue-700",
    suspiciously_old_listing: "bg-amber-100 text-amber-700",
    date_confidence_low: "bg-yellow-100 text-yellow-700",
  };

  return (
    <div className="max-w-6xl mx-auto px-4 py-8">
      <div className="flex items-center justify-between mb-6">
        <div>
          <h1 className="text-2xl font-bold text-gray-900">Review Queue</h1>
          <p className="text-sm text-gray-500 mt-1">
            Dogs flagged for manual review of data accuracy
          </p>
        </div>
        <div className="flex gap-4 text-sm">
          <div className="px-3 py-1.5 bg-amber-50 border border-amber-200 rounded-lg">
            <span className="font-semibold text-amber-700">{openCount || 0}</span>
            <span className="text-amber-600 ml-1">open</span>
          </div>
          <div className="px-3 py-1.5 bg-green-50 border border-green-200 rounded-lg">
            <span className="font-semibold text-green-700">{resolvedCount || 0}</span>
            <span className="text-green-600 ml-1">resolved</span>
          </div>
        </div>
      </div>

      {(!items || items.length === 0) ? (
        <div className="bg-white rounded-xl border p-12 text-center">
          <p className="text-gray-500">No items in review queue.</p>
        </div>
      ) : (
        <div className="space-y-3">
          {items.map((item) => {
            // eslint-disable-next-line @typescript-eslint/no-explicit-any
            const dog = item.dogs as any;
            const shelter = dog?.shelters;
            const sheltInfo = Array.isArray(shelter) ? shelter[0] : shelter;

            return (
              <div
                key={item.id}
                className="bg-white rounded-xl border p-4 flex items-start gap-4 hover:border-blue-300 transition"
              >
                <div className="flex-1 min-w-0">
                  <div className="flex items-center gap-2 mb-1">
                    <span
                      className={`px-2 py-0.5 text-xs font-semibold rounded ${
                        reasonColors[item.reason] || "bg-gray-100 text-gray-700"
                      }`}
                    >
                      {reasonLabels[item.reason] || item.reason}
                    </span>
                    <span className="text-xs text-gray-400">
                      Priority: {item.priority}
                    </span>
                  </div>

                  <Link
                    href={`/dogs/${dog?.id}`}
                    className="text-blue-600 hover:underline font-medium"
                  >
                    {dog?.name || "Unknown Dog"}
                  </Link>
                  <span className="text-sm text-gray-500 ml-2">
                    {dog?.breed_primary}
                  </span>

                  {sheltInfo && (
                    <p className="text-xs text-gray-400 mt-0.5">
                      {sheltInfo.name} - {sheltInfo.city}, {sheltInfo.state_code}
                    </p>
                  )}

                  <div className="flex items-center gap-4 mt-2 text-xs text-gray-500">
                    <span>
                      Intake: {dog?.intake_date ? new Date(dog.intake_date).toLocaleDateString() : "Unknown"}
                    </span>
                    <span>
                      Confidence: <span className="font-medium capitalize">{dog?.date_confidence || "unknown"}</span>
                    </span>
                    <span>
                      Flagged: {new Date(item.created_at).toLocaleDateString()}
                    </span>
                  </div>

                  {item.notes && (
                    <p className="text-xs text-gray-400 mt-1 italic">{item.notes}</p>
                  )}
                </div>

                <div className="flex gap-2 flex-shrink-0">
                  <Link
                    href={`/dogs/${dog?.id}/sources`}
                    className="px-3 py-1.5 text-xs bg-blue-50 text-blue-600 rounded-lg hover:bg-blue-100"
                  >
                    View Sources
                  </Link>
                </div>
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
}
