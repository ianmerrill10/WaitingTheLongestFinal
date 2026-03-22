"use client";

import { useState, useEffect, useCallback } from "react";
import Link from "next/link";

interface RevenueSummary {
  total_revenue: number;
  total_clicks: number;
  total_conversions: number;
  by_partner: Array<{
    name: string;
    clicks: number;
    conversions: number;
    revenue: number;
  }>;
}

export default function RevenueDashboard() {
  const [summary, setSummary] = useState<RevenueSummary | null>(null);
  const [loading, setLoading] = useState(true);

  const fetchData = useCallback(async () => {
    setLoading(true);
    try {
      // In production, this would be an admin API endpoint
      // For now, show placeholder structure
      setSummary({
        total_revenue: 0,
        total_clicks: 0,
        total_conversions: 0,
        by_partner: [],
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

  return (
    <div className="min-h-screen bg-gray-50 py-8">
      <div className="max-w-6xl mx-auto px-4 sm:px-6 lg:px-8">
        <Link
          href="/admin"
          className="text-sm text-green-700 hover:underline mb-1 inline-block"
        >
          &larr; Admin
        </Link>
        <h1 className="text-2xl font-bold mb-6">Revenue Dashboard</h1>

        {loading ? (
          <div className="grid grid-cols-3 gap-4 mb-8">
            {Array.from({ length: 3 }).map((_, i) => (
              <div
                key={i}
                className="bg-white rounded-lg border p-5 animate-pulse"
              >
                <div className="h-4 bg-gray-200 rounded w-20 mb-3" />
                <div className="h-8 bg-gray-200 rounded w-24" />
              </div>
            ))}
          </div>
        ) : summary ? (
          <>
            {/* Summary Stats */}
            <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-8">
              <div className="bg-white rounded-lg border border-gray-200 p-5">
                <p className="text-sm text-gray-500 mb-1">Total Revenue</p>
                <p className="text-2xl font-bold text-green-600">
                  ${summary.total_revenue.toFixed(2)}
                </p>
              </div>
              <div className="bg-white rounded-lg border border-gray-200 p-5">
                <p className="text-sm text-gray-500 mb-1">Total Clicks</p>
                <p className="text-2xl font-bold">
                  {summary.total_clicks.toLocaleString()}
                </p>
              </div>
              <div className="bg-white rounded-lg border border-gray-200 p-5">
                <p className="text-sm text-gray-500 mb-1">Conversions</p>
                <p className="text-2xl font-bold text-purple-600">
                  {summary.total_conversions.toLocaleString()}
                </p>
              </div>
              <div className="bg-white rounded-lg border border-gray-200 p-5">
                <p className="text-sm text-gray-500 mb-1">Conv. Rate</p>
                <p className="text-2xl font-bold">
                  {summary.total_clicks > 0
                    ? (
                        (summary.total_conversions / summary.total_clicks) *
                        100
                      ).toFixed(1)
                    : "0"}
                  %
                </p>
              </div>
            </div>

            {/* Revenue by Partner */}
            <div className="bg-white rounded-lg border border-gray-200 overflow-hidden mb-6">
              <div className="px-5 py-4 border-b border-gray-200">
                <h2 className="text-sm font-semibold text-gray-700">
                  Revenue by Partner
                </h2>
              </div>
              {summary.by_partner.length === 0 ? (
                <div className="p-8 text-center text-gray-500 text-sm">
                  <p className="mb-2">No affiliate data yet.</p>
                  <p className="text-xs text-gray-400">
                    Revenue will appear here once affiliate partners are
                    configured and links start generating clicks.
                  </p>
                </div>
              ) : (
                <table className="w-full">
                  <thead className="bg-gray-50 border-b border-gray-200">
                    <tr>
                      <th className="text-left px-4 py-3 text-xs font-semibold text-gray-500 uppercase">
                        Partner
                      </th>
                      <th className="text-right px-4 py-3 text-xs font-semibold text-gray-500 uppercase">
                        Clicks
                      </th>
                      <th className="text-right px-4 py-3 text-xs font-semibold text-gray-500 uppercase">
                        Conversions
                      </th>
                      <th className="text-right px-4 py-3 text-xs font-semibold text-gray-500 uppercase">
                        Revenue
                      </th>
                      <th className="text-right px-4 py-3 text-xs font-semibold text-gray-500 uppercase">
                        Conv. Rate
                      </th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-gray-100">
                    {summary.by_partner.map((partner) => (
                      <tr key={partner.name} className="hover:bg-gray-50">
                        <td className="px-4 py-3 text-sm font-medium">
                          {partner.name}
                        </td>
                        <td className="px-4 py-3 text-sm text-right">
                          {partner.clicks.toLocaleString()}
                        </td>
                        <td className="px-4 py-3 text-sm text-right">
                          {partner.conversions.toLocaleString()}
                        </td>
                        <td className="px-4 py-3 text-sm text-right font-medium text-green-600">
                          ${partner.revenue.toFixed(2)}
                        </td>
                        <td className="px-4 py-3 text-sm text-right text-gray-500">
                          {partner.clicks > 0
                            ? (
                                (partner.conversions / partner.clicks) *
                                100
                              ).toFixed(1)
                            : "0"}
                          %
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              )}
            </div>

            {/* Setup Guide */}
            <div className="bg-blue-50 rounded-lg border border-blue-200 p-5">
              <h3 className="text-sm font-semibold text-blue-800 mb-2">
                Affiliate Setup Checklist
              </h3>
              <ul className="text-sm text-blue-700 space-y-1">
                <li>
                  1. Amazon Associates ID configured: <strong>waitingthelon-20</strong>
                </li>
                <li>2. Apply for Chewy affiliate program at chewy.com/affiliates</li>
                <li>3. Apply for pet insurance affiliate programs (Lemonade, Healthy Paws)</li>
                <li>4. Add products to the affiliate_products table</li>
                <li>5. Products will auto-appear on dog profile pages</li>
              </ul>
            </div>
          </>
        ) : null}
      </div>
    </div>
  );
}
