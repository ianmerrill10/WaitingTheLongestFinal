"use client";

import { useState, useEffect } from "react";
import Link from "next/link";

interface ApiKeyDisplay {
  id: string;
  key_prefix: string;
  label: string;
  environment: string;
  scopes: string[];
  is_active: boolean;
  last_used_at: string | null;
  usage_count_today: number;
  usage_count_total: number;
  created_at: string;
}

export default function DashboardKeysPage() {
  const [keys, setKeys] = useState<ApiKeyDisplay[]>([]);
  const [newKey, setNewKey] = useState<string | null>(null);

  useEffect(() => {
    const shelterId = document.cookie
      .split("; ")
      .find((c) => c.startsWith("wtl_shelter_id="))
      ?.split("=")[1];
    if (!shelterId) return;
    fetch(`/api/admin/partner?shelter_id=${shelterId}`)
      .then((r) => r.ok ? r.json() : null)
      .then((data) => { if (data?.keys) setKeys(data.keys); })
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
            <h1 className="text-2xl font-bold">API Keys</h1>
            <p className="text-gray-600 text-sm mt-1">
              Manage API keys for programmatic access to your dog listings.
            </p>
          </div>
          <button className="px-4 py-2 bg-green-700 text-white rounded-lg text-sm hover:bg-green-800">
            Generate New Key
          </button>
        </div>

        {/* New Key Alert */}
        {newKey && (
          <div className="bg-yellow-50 border border-yellow-200 rounded-lg p-4 mb-6">
            <h3 className="font-bold text-yellow-800 mb-2">
              Save Your New API Key
            </h3>
            <p className="text-sm text-yellow-700 mb-3">
              This key will only be shown once. Copy it now and store it securely.
            </p>
            <code className="block bg-white border border-yellow-300 rounded p-3 font-mono text-sm break-all">
              {newKey}
            </code>
            <button
              onClick={() => {
                navigator.clipboard.writeText(newKey);
              }}
              className="mt-3 px-3 py-1 bg-yellow-100 text-yellow-800 rounded text-sm hover:bg-yellow-200"
            >
              Copy to Clipboard
            </button>
          </div>
        )}

        {/* Keys Table */}
        <div className="bg-white rounded-lg border border-gray-200 overflow-hidden">
          {keys.length === 0 ? (
            <div className="p-12 text-center text-gray-500">
              <p className="text-lg mb-2">No API keys yet</p>
              <p className="text-sm">
                Generate your first API key to start integrating with our REST API.
              </p>
            </div>
          ) : (
            <table className="w-full">
              <thead className="bg-gray-50 border-b border-gray-200">
                <tr>
                  <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">Key</th>
                  <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">Label</th>
                  <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">Env</th>
                  <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">Scopes</th>
                  <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">Usage Today</th>
                  <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">Last Used</th>
                  <th className="text-right px-4 py-3 text-sm font-medium text-gray-600">Actions</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-gray-100">
                {keys.map((key) => (
                  <tr key={key.id} className="hover:bg-gray-50">
                    <td className="px-4 py-3 font-mono text-sm">{key.key_prefix}...</td>
                    <td className="px-4 py-3">{key.label}</td>
                    <td className="px-4 py-3">
                      <span className={`px-2 py-0.5 rounded text-xs font-medium ${
                        key.environment === "production"
                          ? "bg-green-100 text-green-700"
                          : "bg-gray-100 text-gray-600"
                      }`}>
                        {key.environment}
                      </span>
                    </td>
                    <td className="px-4 py-3 text-sm text-gray-600">
                      {key.scopes.join(", ")}
                    </td>
                    <td className="px-4 py-3 text-sm">{key.usage_count_today}</td>
                    <td className="px-4 py-3 text-sm text-gray-500">
                      {key.last_used_at
                        ? new Date(key.last_used_at).toLocaleDateString()
                        : "Never"}
                    </td>
                    <td className="px-4 py-3 text-right">
                      <button className="text-sm text-blue-600 hover:underline mr-3">
                        Rotate
                      </button>
                      <button className="text-sm text-red-600 hover:underline">
                        Revoke
                      </button>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>

        {/* Usage Info */}
        <div className="mt-6 bg-white rounded-lg border border-gray-200 p-6">
          <h2 className="font-bold mb-3">Rate Limits</h2>
          <div className="grid grid-cols-3 gap-4 text-sm">
            <div>
              <div className="text-gray-500">Per Minute</div>
              <div className="font-bold">60 requests</div>
            </div>
            <div>
              <div className="text-gray-500">Per Hour</div>
              <div className="font-bold">1,000 requests</div>
            </div>
            <div>
              <div className="text-gray-500">Per Day</div>
              <div className="font-bold">10,000 requests</div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
