"use client";

import { useEffect, useState } from "react";
import Link from "next/link";

interface DashboardData {
  shelter: {
    id: string;
    name: string;
    city: string;
    state_code: string;
    partner_status: string;
    partner_tier: string;
  } | null;
  stats: {
    total_dogs: number;
    available_dogs: number;
    adopted_dogs: number;
    urgent_dogs: number;
    total_views: number;
  };
}

export default function DashboardPage() {
  const [data, setData] = useState<DashboardData | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    loadDashboard();
  }, []);

  const loadDashboard = async () => {
    try {
      // In production, this would use the session cookie to authenticate
      // For now, show a placeholder dashboard
      setData({
        shelter: null,
        stats: {
          total_dogs: 0,
          available_dogs: 0,
          adopted_dogs: 0,
          urgent_dogs: 0,
          total_views: 0,
        },
      });
    } catch {
      // Handle error
    } finally {
      setLoading(false);
    }
  };

  if (loading) {
    return (
      <div className="min-h-screen bg-gray-50 flex items-center justify-center">
        <div className="text-gray-500">Loading dashboard...</div>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-gray-50 py-8">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        {/* Header */}
        <div className="flex items-center justify-between mb-8">
          <div>
            <h1 className="text-2xl font-bold">
              {data?.shelter?.name || "Shelter Dashboard"}
            </h1>
            <p className="text-gray-600">
              Manage your dogs, API keys, and integrations
            </p>
          </div>
          <Link
            href="/partners/login"
            className="text-sm text-gray-500 hover:text-gray-700"
          >
            Sign Out
          </Link>
        </div>

        {/* Stats Grid */}
        <div className="grid grid-cols-2 md:grid-cols-5 gap-4 mb-8">
          <StatCard label="Total Dogs" value={data?.stats.total_dogs || 0} />
          <StatCard
            label="Available"
            value={data?.stats.available_dogs || 0}
            color="green"
          />
          <StatCard
            label="Adopted"
            value={data?.stats.adopted_dogs || 0}
            color="blue"
          />
          <StatCard
            label="Urgent"
            value={data?.stats.urgent_dogs || 0}
            color="red"
          />
          <StatCard
            label="Total Views"
            value={data?.stats.total_views || 0}
          />
        </div>

        {/* Quick Actions */}
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
          <DashboardCard
            title="Dog Management"
            description="View, add, edit, and remove your dog listings. Upload photos and update availability."
            href="/partners/dashboard/dogs"
            icon="🐕"
          />
          <DashboardCard
            title="API Keys"
            description="Generate and manage API keys for programmatic access to your listings."
            href="/partners/dashboard/keys"
            icon="🔑"
          />
          <DashboardCard
            title="Webhooks"
            description="Register webhook endpoints to receive real-time notifications about your dogs."
            href="/partners/dashboard/webhooks"
            icon="🔔"
          />
          <DashboardCard
            title="Communications"
            description="View your message history with WaitingTheLongest.com."
            href="/partners/dashboard/communications"
            icon="💬"
          />
          <DashboardCard
            title="Analytics"
            description="Track views, inquiries, and adoption metrics for your listings."
            href="/partners/dashboard/analytics"
            icon="📊"
          />
          <DashboardCard
            title="CSV Import"
            description="Upload a CSV file to bulk-create or update dog listings."
            href="/partners/dashboard/dogs"
            icon="📤"
          />
        </div>

        {/* API Documentation Link */}
        <div className="mt-8 bg-white rounded-lg border border-gray-200 p-6">
          <h2 className="text-lg font-bold mb-2">API Documentation</h2>
          <p className="text-gray-600 mb-4">
            Integrate your shelter management software with our REST API. Full
            CRUD operations, webhook support, and CSV import.
          </p>
          <div className="flex gap-4">
            <Link
              href="/legal/api-terms"
              className="text-green-700 font-semibold hover:underline text-sm"
            >
              API Usage Agreement
            </Link>
            <span className="text-gray-300">|</span>
            <span className="text-sm text-gray-500">
              Base URL: <code className="bg-gray-100 px-1 rounded">https://waitingthelongest.com/api/v1</code>
            </span>
          </div>
        </div>
      </div>
    </div>
  );
}

function StatCard({
  label,
  value,
  color,
}: {
  label: string;
  value: number;
  color?: string;
}) {
  const colorClass =
    color === "green"
      ? "text-green-600"
      : color === "blue"
        ? "text-blue-600"
        : color === "red"
          ? "text-red-600"
          : "text-gray-900";

  return (
    <div className="bg-white rounded-lg border border-gray-200 p-4">
      <div className={`text-2xl font-bold ${colorClass}`}>
        {value.toLocaleString()}
      </div>
      <div className="text-sm text-gray-500">{label}</div>
    </div>
  );
}

function DashboardCard({
  title,
  description,
  href,
  icon,
}: {
  title: string;
  description: string;
  href: string;
  icon: string;
}) {
  return (
    <Link
      href={href}
      className="bg-white rounded-lg border border-gray-200 p-6 hover:border-green-400 hover:shadow-md transition-all group"
    >
      <div className="text-2xl mb-3">{icon}</div>
      <h3 className="text-lg font-bold mb-1 group-hover:text-green-700 transition-colors">
        {title}
      </h3>
      <p className="text-sm text-gray-600">{description}</p>
    </Link>
  );
}
