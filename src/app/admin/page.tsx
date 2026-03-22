"use client";

import Link from "next/link";

const adminSections = [
  {
    title: "Data Dashboard",
    description: "Verification progress, date accuracy, daily trends, and audit history.",
    href: "/admin/dashboard",
    count: null,
    color: "bg-emerald-50 border-emerald-200 text-emerald-700",
  },
  {
    title: "Onboarding Pipeline",
    description: "Review, approve, and advance shelter applications through the pipeline.",
    href: "/admin/onboarding",
    count: null,
    color: "bg-green-50 border-green-200 text-green-700",
  },
  {
    title: "Social Media",
    description: "Content generator, scheduling, and cross-platform analytics.",
    href: "/admin/social",
    count: null,
    color: "bg-blue-50 border-blue-200 text-blue-700",
  },
  {
    title: "Revenue",
    description: "Affiliate earnings, click tracking, and product performance.",
    href: "/admin/revenue",
    count: null,
    color: "bg-purple-50 border-purple-200 text-purple-700",
  },
  {
    title: "Outreach Campaigns",
    description: "AI-powered outreach to shelters across America.",
    href: "/admin/outreach",
    count: null,
    color: "bg-orange-50 border-orange-200 text-orange-700",
  },
];

export default function AdminPage() {
  return (
    <div className="min-h-screen bg-gray-50 py-8">
      <div className="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8">
        <h1 className="text-2xl font-bold mb-2">Admin Dashboard</h1>
        <p className="text-gray-600 mb-8">
          Internal management tools for WaitingTheLongest.com
        </p>

        <div className="grid sm:grid-cols-2 gap-4">
          {adminSections.map((section) => (
            <Link
              key={section.title}
              href={section.href}
              className={`block rounded-xl border p-6 ${section.color} hover:shadow-md transition-shadow`}
            >
              <h2 className="text-lg font-bold mb-1">{section.title}</h2>
              <p className="text-sm opacity-80">{section.description}</p>
            </Link>
          ))}
        </div>

        <div className="mt-8 pt-6 border-t border-gray-200">
          <h2 className="text-sm font-semibold text-gray-500 uppercase mb-3">
            Quick Links
          </h2>
          <div className="flex flex-wrap gap-3 text-sm">
            <Link href="/api/debug" className="text-green-700 hover:underline">
              Debug Dashboard
            </Link>
            <Link href="/api/audit" className="text-green-700 hover:underline">
              Run Audit
            </Link>
            <Link href="/api/backup" className="text-green-700 hover:underline">
              Create Backup
            </Link>
            <Link
              href="/partners"
              className="text-green-700 hover:underline"
            >
              Partner Portal
            </Link>
          </div>
        </div>
      </div>
    </div>
  );
}
