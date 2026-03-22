"use client";

import { useState, useEffect, useCallback } from "react";
import Link from "next/link";

interface CampaignSummary {
  id: string;
  name: string;
  campaign_type: string;
  status: string;
  total_targets: number;
  total_sent: number;
  total_responded: number;
  total_converted: number;
  created_at: string;
}

interface OutreachStats {
  total_shelters: number;
  with_email: number;
  campaigns_active: number;
  total_sent_alltime: number;
}

export default function OutreachDashboard() {
  const [campaigns, setCampaigns] = useState<CampaignSummary[]>([]);
  const [stats, setStats] = useState<OutreachStats | null>(null);
  const [loading, setLoading] = useState(true);
  const [creating, setCreating] = useState(false);

  const fetchData = useCallback(async () => {
    setLoading(true);
    try {
      // Placeholder — in production, this would be an admin API
      setCampaigns([]);
      setStats({
        total_shelters: 44947,
        with_email: 0,
        campaigns_active: 0,
        total_sent_alltime: 0,
      });
    } catch {
      // fail silently
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchData();
  }, [fetchData]);

  const handleCreateCampaign = async () => {
    setCreating(true);
    try {
      const name = prompt("Campaign name:");
      if (!name) return;

      const state = prompt("State filter (e.g., TX) or leave blank for all:");
      const minDogs = prompt("Minimum dogs (default: 0):");

      const res = await fetch("/api/outreach/process", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          action: "create_campaign",
          name,
          campaign_type: "initial_outreach",
          state: state || undefined,
          min_dogs: minDogs ? parseInt(minDogs) : undefined,
          max_targets: 500,
        }),
      });

      if (res.ok) {
        const data = await res.json();
        alert(
          `Campaign created! ${data.targets_added} shelters added to queue.`
        );
        fetchData();
      }
    } catch {
      // fail silently
    } finally {
      setCreating(false);
    }
  };

  return (
    <div className="min-h-screen bg-gray-50 py-8">
      <div className="max-w-6xl mx-auto px-4 sm:px-6 lg:px-8">
        <Link
          href="/admin"
          className="text-sm text-green-700 hover:underline mb-1 inline-block"
        >
          &larr; Admin
        </Link>
        <div className="flex items-center justify-between mb-6">
          <h1 className="text-2xl font-bold">AI Outreach</h1>
          <div className="flex gap-2">
            <Link
              href="/admin/outreach/campaigns"
              className="text-sm bg-white border border-gray-200 rounded-lg px-3 py-1.5 hover:bg-gray-50"
            >
              Campaigns
            </Link>
            <button
              onClick={handleCreateCampaign}
              disabled={creating}
              className="text-sm bg-green-700 text-white rounded-lg px-3 py-1.5 hover:bg-green-800 disabled:opacity-50"
            >
              {creating ? "Creating..." : "New Campaign"}
            </button>
          </div>
        </div>

        {/* Stats */}
        {stats && (
          <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-8">
            <div className="bg-white rounded-lg border border-gray-200 p-5">
              <p className="text-sm text-gray-500 mb-1">Total Shelters</p>
              <p className="text-2xl font-bold">
                {stats.total_shelters.toLocaleString()}
              </p>
            </div>
            <div className="bg-white rounded-lg border border-gray-200 p-5">
              <p className="text-sm text-gray-500 mb-1">With Email</p>
              <p className="text-2xl font-bold text-green-600">
                {stats.with_email.toLocaleString()}
              </p>
            </div>
            <div className="bg-white rounded-lg border border-gray-200 p-5">
              <p className="text-sm text-gray-500 mb-1">Active Campaigns</p>
              <p className="text-2xl font-bold text-blue-600">
                {stats.campaigns_active}
              </p>
            </div>
            <div className="bg-white rounded-lg border border-gray-200 p-5">
              <p className="text-sm text-gray-500 mb-1">Emails Sent</p>
              <p className="text-2xl font-bold text-purple-600">
                {stats.total_sent_alltime.toLocaleString()}
              </p>
            </div>
          </div>
        )}

        {/* Outreach Strategy */}
        <div className="bg-white rounded-lg border border-gray-200 p-6 mb-6">
          <h2 className="text-sm font-semibold text-gray-700 mb-3">
            Outreach Tiers
          </h2>
          <div className="grid sm:grid-cols-2 md:grid-cols-4 gap-3">
            <div className="p-3 rounded-lg bg-red-50 border border-red-200">
              <div className="text-sm font-bold text-red-700">Tier 1</div>
              <div className="text-xs text-red-600">~1,500 kill shelters</div>
              <div className="text-xs text-red-500 mt-1">Score 70+</div>
            </div>
            <div className="p-3 rounded-lg bg-orange-50 border border-orange-200">
              <div className="text-sm font-bold text-orange-700">Tier 2</div>
              <div className="text-xs text-orange-600">
                ~3,000 high-volume (50+ dogs)
              </div>
              <div className="text-xs text-orange-500 mt-1">Score 40-69</div>
            </div>
            <div className="p-3 rounded-lg bg-yellow-50 border border-yellow-200">
              <div className="text-sm font-bold text-yellow-700">Tier 3</div>
              <div className="text-xs text-yellow-600">
                ~10,000 active rescues
              </div>
              <div className="text-xs text-yellow-500 mt-1">Score 20-39</div>
            </div>
            <div className="p-3 rounded-lg bg-gray-50 border border-gray-200">
              <div className="text-sm font-bold text-gray-700">Tier 4</div>
              <div className="text-xs text-gray-600">
                ~30,000 smaller rescues
              </div>
              <div className="text-xs text-gray-500 mt-1">Score &lt;20</div>
            </div>
          </div>
        </div>

        {/* Campaigns List */}
        <div className="bg-white rounded-lg border border-gray-200 overflow-hidden">
          <div className="px-5 py-4 border-b border-gray-200">
            <h2 className="text-sm font-semibold text-gray-700">
              Recent Campaigns
            </h2>
          </div>
          {loading ? (
            <div className="p-8 text-center text-gray-400">Loading...</div>
          ) : campaigns.length === 0 ? (
            <div className="p-8 text-center text-gray-500">
              <p className="mb-2">No campaigns yet.</p>
              <p className="text-sm text-gray-400">
                Create a campaign to start reaching out to shelters across
                America. Each campaign targets shelters by state, size, or
                priority tier.
              </p>
            </div>
          ) : (
            <table className="w-full">
              <thead className="bg-gray-50 border-b border-gray-200">
                <tr>
                  <th className="text-left px-4 py-3 text-xs font-semibold text-gray-500 uppercase">
                    Campaign
                  </th>
                  <th className="text-left px-4 py-3 text-xs font-semibold text-gray-500 uppercase">
                    Status
                  </th>
                  <th className="text-right px-4 py-3 text-xs font-semibold text-gray-500 uppercase">
                    Targets
                  </th>
                  <th className="text-right px-4 py-3 text-xs font-semibold text-gray-500 uppercase">
                    Sent
                  </th>
                  <th className="text-right px-4 py-3 text-xs font-semibold text-gray-500 uppercase">
                    Responded
                  </th>
                  <th className="text-right px-4 py-3 text-xs font-semibold text-gray-500 uppercase">
                    Converted
                  </th>
                </tr>
              </thead>
              <tbody className="divide-y divide-gray-100">
                {campaigns.map((c) => (
                  <tr key={c.id} className="hover:bg-gray-50">
                    <td className="px-4 py-3">
                      <div className="text-sm font-medium">{c.name}</div>
                      <div className="text-xs text-gray-500">
                        {new Date(c.created_at).toLocaleDateString()}
                      </div>
                    </td>
                    <td className="px-4 py-3">
                      <span
                        className={`px-2 py-0.5 rounded text-xs font-medium ${
                          c.status === "active"
                            ? "bg-green-100 text-green-700"
                            : c.status === "draft"
                              ? "bg-gray-100 text-gray-600"
                              : "bg-blue-100 text-blue-700"
                        }`}
                      >
                        {c.status}
                      </span>
                    </td>
                    <td className="px-4 py-3 text-sm text-right">
                      {c.total_targets}
                    </td>
                    <td className="px-4 py-3 text-sm text-right">
                      {c.total_sent}
                    </td>
                    <td className="px-4 py-3 text-sm text-right">
                      {c.total_responded}
                    </td>
                    <td className="px-4 py-3 text-sm text-right font-medium text-green-600">
                      {c.total_converted}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>

        {/* Setup Info */}
        <div className="mt-6 bg-blue-50 rounded-lg border border-blue-200 p-5">
          <h3 className="text-sm font-semibold text-blue-800 mb-2">
            Setup Requirements
          </h3>
          <ul className="text-sm text-blue-700 space-y-1">
            <li>1. Configure SENDGRID_API_KEY in Vercel environment variables</li>
            <li>2. Set up SendGrid domain authentication for waitingthelongest.com</li>
            <li>3. Configure SendGrid Inbound Parse webhook to /api/outreach/webhook</li>
            <li>4. Create a campaign and populate targets</li>
            <li>5. Activate campaign and process queue</li>
          </ul>
        </div>
      </div>
    </div>
  );
}
