"use client";

import { useState, useEffect, useCallback } from "react";
import Link from "next/link";

interface Campaign {
  id: string;
  name: string;
  campaign_type: string;
  status: string;
  total_targets: number;
  total_sent: number;
  total_responded: number;
  total_converted: number;
  total_opted_out: number;
  total_bounced: number;
  created_at: string;
  updated_at: string;
}

export default function CampaignManagementPage() {
  const [campaigns, setCampaigns] = useState<Campaign[]>([]);
  const [loading, setLoading] = useState(true);
  const [processing, setProcessing] = useState<string | null>(null);

  const fetchCampaigns = useCallback(async () => {
    setLoading(true);
    try {
      const res = await fetch("/api/admin/campaigns");
      if (res.ok) {
        const data = await res.json();
        setCampaigns(data.campaigns || []);
      }
    } catch {
      // fail silently
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchCampaigns();
  }, [fetchCampaigns]);

  const handleProcess = async (campaignId: string) => {
    setProcessing(campaignId);
    try {
      const res = await fetch("/api/outreach/process", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          action: "process",
          campaign_id: campaignId,
          batch_size: 20,
        }),
      });

      if (res.ok) {
        const data = await res.json();
        alert(
          `Processed: ${data.processed} | Sent: ${data.sent} | Skipped: ${data.skipped} | Errors: ${data.errors}`
        );
        fetchCampaigns();
      }
    } catch {
      alert("Processing failed");
    } finally {
      setProcessing(null);
    }
  };

  return (
    <div className="min-h-screen bg-gray-50 py-8">
      <div className="max-w-6xl mx-auto px-4 sm:px-6 lg:px-8">
        <Link
          href="/admin/outreach"
          className="text-sm text-green-700 hover:underline mb-1 inline-block"
        >
          &larr; Outreach
        </Link>
        <h1 className="text-2xl font-bold mb-6">Campaign Management</h1>

        {loading ? (
          <div className="bg-white rounded-lg border p-12 text-center text-gray-400">
            Loading campaigns...
          </div>
        ) : campaigns.length === 0 ? (
          <div className="bg-white rounded-lg border border-gray-200 p-12 text-center text-gray-500">
            <p className="text-lg mb-2">No campaigns yet</p>
            <p className="text-sm mb-4">
              Create your first campaign from the{" "}
              <Link
                href="/admin/outreach"
                className="text-green-700 hover:underline"
              >
                outreach dashboard
              </Link>
              .
            </p>
          </div>
        ) : (
          <div className="space-y-4">
            {campaigns.map((campaign) => (
              <div
                key={campaign.id}
                className="bg-white rounded-lg border border-gray-200 p-6"
              >
                <div className="flex items-start justify-between mb-4">
                  <div>
                    <h2 className="text-lg font-semibold">{campaign.name}</h2>
                    <p className="text-xs text-gray-500">
                      Created{" "}
                      {new Date(campaign.created_at).toLocaleDateString()} ·{" "}
                      {campaign.campaign_type}
                    </p>
                  </div>
                  <div className="flex items-center gap-2">
                    <span
                      className={`px-2 py-0.5 rounded text-xs font-medium ${
                        campaign.status === "active"
                          ? "bg-green-100 text-green-700"
                          : campaign.status === "draft"
                            ? "bg-gray-100 text-gray-600"
                            : campaign.status === "completed"
                              ? "bg-blue-100 text-blue-700"
                              : "bg-red-100 text-red-700"
                      }`}
                    >
                      {campaign.status}
                    </span>
                    {campaign.status === "active" && (
                      <button
                        onClick={() => handleProcess(campaign.id)}
                        disabled={processing === campaign.id}
                        className="text-xs bg-green-700 text-white rounded px-3 py-1 hover:bg-green-800 disabled:opacity-50"
                      >
                        {processing === campaign.id
                          ? "Processing..."
                          : "Process Queue"}
                      </button>
                    )}
                  </div>
                </div>

                {/* Campaign Stats */}
                <div className="grid grid-cols-3 sm:grid-cols-6 gap-3">
                  <div>
                    <div className="text-lg font-bold">
                      {campaign.total_targets}
                    </div>
                    <div className="text-xs text-gray-500">Targets</div>
                  </div>
                  <div>
                    <div className="text-lg font-bold text-blue-600">
                      {campaign.total_sent}
                    </div>
                    <div className="text-xs text-gray-500">Sent</div>
                  </div>
                  <div>
                    <div className="text-lg font-bold text-green-600">
                      {campaign.total_responded}
                    </div>
                    <div className="text-xs text-gray-500">Responded</div>
                  </div>
                  <div>
                    <div className="text-lg font-bold text-purple-600">
                      {campaign.total_converted}
                    </div>
                    <div className="text-xs text-gray-500">Converted</div>
                  </div>
                  <div>
                    <div className="text-lg font-bold text-orange-600">
                      {campaign.total_opted_out}
                    </div>
                    <div className="text-xs text-gray-500">Opted Out</div>
                  </div>
                  <div>
                    <div className="text-lg font-bold text-red-600">
                      {campaign.total_bounced}
                    </div>
                    <div className="text-xs text-gray-500">Bounced</div>
                  </div>
                </div>

                {/* Progress bar */}
                {campaign.total_targets > 0 && (
                  <div className="mt-4">
                    <div className="flex items-center justify-between text-xs text-gray-500 mb-1">
                      <span>Progress</span>
                      <span>
                        {Math.round(
                          (campaign.total_sent / campaign.total_targets) * 100
                        )}
                        %
                      </span>
                    </div>
                    <div className="w-full bg-gray-100 rounded-full h-2">
                      <div
                        className="bg-green-500 rounded-full h-2 transition-all"
                        style={{
                          width: `${Math.min(
                            (campaign.total_sent / campaign.total_targets) *
                              100,
                            100
                          )}%`,
                        }}
                      />
                    </div>
                  </div>
                )}
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
