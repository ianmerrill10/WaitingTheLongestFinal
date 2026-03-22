"use client";

import { useState, useEffect, useCallback } from "react";
import Link from "next/link";
import { useToast } from "@/components/ui/Toast";

interface WebhookEndpoint {
  id: string;
  url: string;
  events: string[];
  is_active: boolean;
  total_deliveries: number;
  total_successes: number;
  total_failures: number;
  last_delivery_at: string | null;
}

const AVAILABLE_EVENTS = [
  "dog.created", "dog.updated", "dog.adopted", "dog.urgent",
  "dog.viewed", "dog.inquiry_received", "dog.shared", "account.updated",
];

export default function DashboardWebhooksPage() {
  const [endpoints, setEndpoints] = useState<WebhookEndpoint[]>([]);
  const [loading, setLoading] = useState(true);
  const [adding, setAdding] = useState(false);
  const [webhookSecret, setWebhookSecret] = useState<string | null>(null);
  const { toast } = useToast();

  const loadWebhooks = useCallback(async () => {
    try {
      const shelterId = document.cookie
        .split("; ")
        .find((c) => c.startsWith("wtl_shelter_id="))
        ?.split("=")[1];
      if (!shelterId) return;
      const res = await fetch(`/api/admin/partner?shelter_id=${shelterId}`);
      if (res.ok) {
        const data = await res.json();
        if (data?.webhooks) setEndpoints(data.webhooks);
      }
    } catch {
      toast("Failed to load webhooks", "error");
    } finally {
      setLoading(false);
    }
  }, [toast]);

  useEffect(() => {
    loadWebhooks();
  }, [loadWebhooks]);

  const handleAdd = async () => {
    const url = prompt("Webhook URL (must start with https://):");
    if (!url) return;

    setAdding(true);
    try {
      const res = await fetch("/api/partners/webhooks", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ url, events: AVAILABLE_EVENTS }),
      });

      if (res.ok) {
        const data = await res.json();
        setWebhookSecret(data.secret);
        toast("Webhook endpoint created", "success");
        loadWebhooks();
      } else {
        const err = await res.json();
        toast(err.error || "Failed to create webhook", "error");
      }
    } catch {
      toast("Failed to create webhook", "error");
    } finally {
      setAdding(false);
    }
  };

  const handleDelete = async (webhookId: string) => {
    if (!confirm("Delete this webhook endpoint?")) return;

    try {
      const res = await fetch(`/api/partners/webhooks?id=${webhookId}`, {
        method: "DELETE",
      });

      if (res.ok) {
        toast("Webhook deleted", "success");
        loadWebhooks();
      } else {
        toast("Failed to delete webhook", "error");
      }
    } catch {
      toast("Failed to delete webhook", "error");
    }
  };

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
          <button
            type="button"
            onClick={handleAdd}
            disabled={adding}
            className="px-4 py-2 bg-green-700 text-white rounded-lg text-sm hover:bg-green-800 disabled:opacity-50"
          >
            {adding ? "Adding..." : "Add Endpoint"}
          </button>
        </div>

        {/* Webhook Secret Alert */}
        {webhookSecret && (
          <div className="bg-yellow-50 border border-yellow-200 rounded-lg p-4 mb-6">
            <h3 className="font-bold text-yellow-800 mb-2">
              Save Your Webhook Secret
            </h3>
            <p className="text-sm text-yellow-700 mb-3">
              Use this secret to verify webhook signatures. It will not be shown again.
            </p>
            <code className="block bg-white border border-yellow-300 rounded p-3 font-mono text-sm break-all">
              {webhookSecret}
            </code>
            <button
              type="button"
              onClick={() => {
                navigator.clipboard.writeText(webhookSecret);
                toast("Copied to clipboard", "success");
              }}
              className="mt-3 px-3 py-1 bg-yellow-100 text-yellow-800 rounded text-sm hover:bg-yellow-200"
            >
              Copy to Clipboard
            </button>
          </div>
        )}

        {/* Available Events */}
        <div className="bg-white rounded-lg border border-gray-200 p-4 mb-6">
          <h3 className="font-semibold text-sm mb-2">Available Events</h3>
          <div className="flex flex-wrap gap-2">
            {AVAILABLE_EVENTS.map((event) => (
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
          {loading ? (
            <div className="bg-white rounded-lg border border-gray-200 p-12 text-center text-gray-400">
              Loading...
            </div>
          ) : endpoints.length === 0 ? (
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
                  <button
                    type="button"
                    onClick={() => handleDelete(ep.id)}
                    className="text-sm text-red-600 hover:underline"
                  >
                    Delete
                  </button>
                </div>
                <div className="flex gap-4 text-xs text-gray-500">
                  <span>
                    {ep.total_deliveries} deliveries ({ep.total_successes}{" "}
                    OK, {ep.total_failures} failed)
                  </span>
                  <span>Events: {ep.events?.join(", ") || "all"}</span>
                  {ep.last_delivery_at && (
                    <span>
                      Last: {new Date(ep.last_delivery_at).toLocaleString()}
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
