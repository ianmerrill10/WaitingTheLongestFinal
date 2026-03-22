"use client";

import { useState, useEffect } from "react";
import Link from "next/link";

export default function DashboardWebhooksPage() {
  const [endpoints, setEndpoints] = useState<Array<{
    id: string;
    url: string;
    events: string[];
    is_active: boolean;
    total_deliveries: number;
    total_successes: number;
    total_failures: number;
    last_delivery_at: string | null;
  }>>([]);

  useEffect(() => {
    const shelterId = document.cookie
      .split("; ")
      .find((c) => c.startsWith("wtl_shelter_id="))
      ?.split("=")[1];
    if (!shelterId) return;
    fetch(`/api/admin/partner?shelter_id=${shelterId}`)
      .then((r) => r.ok ? r.json() : null)
      .then((data) => { if (data?.webhooks) setEndpoints(data.webhooks); })
      .catch(() => {});
  }, []);

  return (
    <div className="min-h-screen bg-gray-50 py-8">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="flex items-center justify-between mb-6">
          <div>
            <Link
              href="/partners/dashboard"
              className="text-sm text-green-700 hover:underline mb-1 inline-block"
            >
              &larr; Dashboard
            </Link>
            <h1 className="text-2xl font-bold">Webhooks</h1>
            <p className="text-gray-600 text-sm mt-1">
              Register endpoints to receive real-time notifications about your dogs.
            </p>
          </div>
          <button className="px-4 py-2 bg-green-700 text-white rounded-lg text-sm hover:bg-green-800">
            Add Endpoint
          </button>
        </div>

        {/* Available Events */}
        <div className="bg-white rounded-lg border border-gray-200 p-4 mb-6">
          <h3 className="font-semibold text-sm mb-2">Available Events</h3>
          <div className="flex flex-wrap gap-2">
            {[
              "dog.created", "dog.updated", "dog.adopted", "dog.urgent",
              "dog.viewed", "dog.inquiry_received", "dog.shared", "account.updated",
            ].map((event) => (
              <span
                key={event}
                className="px-2 py-1 bg-gray-100 rounded text-xs font-mono"
              >
                {event}
              </span>
            ))}
          </div>
        </div>

        {/* Endpoints */}
        <div className="space-y-4">
          {endpoints.length === 0 ? (
            <div className="bg-white rounded-lg border border-gray-200 p-12 text-center text-gray-500">
              <p className="text-lg mb-2">No webhook endpoints registered</p>
              <p className="text-sm">
                Add an endpoint to start receiving real-time notifications when
                dogs are viewed, adopted, or need attention.
              </p>
            </div>
          ) : (
            endpoints.map((ep) => (
              <div
                key={ep.id}
                className="bg-white rounded-lg border border-gray-200 p-4"
              >
                <div className="flex items-center justify-between mb-2">
                  <div className="flex items-center gap-2">
                    <span
                      className={`w-2 h-2 rounded-full ${
                        ep.is_active ? "bg-green-500" : "bg-gray-400"
                      }`}
                    />
                    <code className="text-sm font-mono">{ep.url}</code>
                  </div>
                  <div className="flex gap-2">
                    <button className="text-sm text-blue-600 hover:underline">
                      Test
                    </button>
                    <button className="text-sm text-red-600 hover:underline">
                      Delete
                    </button>
                  </div>
                </div>
                <div className="flex gap-4 text-xs text-gray-500">
                  <span>
                    {ep.total_deliveries} deliveries ({ep.total_successes}{" "}
                    OK, {ep.total_failures} failed)
                  </span>
                  <span>Events: {ep.events.join(", ")}</span>
                  {ep.last_delivery_at && (
                    <span>
                      Last delivery:{" "}
                      {new Date(ep.last_delivery_at).toLocaleString()}
                    </span>
                  )}
                </div>
              </div>
            ))
          )}
        </div>
      </div>
    </div>
  );
}
