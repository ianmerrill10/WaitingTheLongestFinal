"use client";

import { useState, useEffect } from "react";
import Link from "next/link";

export default function DashboardCommunicationsPage() {
  const [communications, setCommunications] = useState<Array<{
    id: string;
    comm_type: string;
    direction: string;
    subject: string;
    status: string;
    created_at: string;
  }>>([]);

  useEffect(() => {
    const shelterId = document.cookie
      .split("; ")
      .find((c) => c.startsWith("wtl_shelter_id="))
      ?.split("=")[1];
    if (!shelterId) return;
    fetch(`/api/admin/partner?shelter_id=${shelterId}`)
      .then((r) => r.ok ? r.json() : null)
      .then((data) => { if (data?.communications) setCommunications(data.communications); })
      .catch(() => {});
  }, []);

  return (
    <div className="min-h-screen bg-gray-50 py-8">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <Link
          href="/partners/dashboard"
          className="text-sm text-green-700 hover:underline mb-1 inline-block"
        >
          &larr; Dashboard
        </Link>
        <h1 className="text-2xl font-bold mb-6">Communications</h1>

        <div className="bg-white rounded-lg border border-gray-200 overflow-hidden">
          {communications.length === 0 ? (
            <div className="p-12 text-center text-gray-500">
              <p className="text-lg mb-2">No communications yet</p>
              <p className="text-sm">
                Your message history with WaitingTheLongest.com will appear
                here.
              </p>
            </div>
          ) : (
            <table className="w-full">
              <thead className="bg-gray-50 border-b border-gray-200">
                <tr>
                  <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">
                    Type
                  </th>
                  <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">
                    Direction
                  </th>
                  <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">
                    Subject
                  </th>
                  <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">
                    Status
                  </th>
                  <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">
                    Date
                  </th>
                </tr>
              </thead>
              <tbody className="divide-y divide-gray-100">
                {communications.map((comm) => (
                  <tr key={comm.id} className="hover:bg-gray-50">
                    <td className="px-4 py-3 text-sm">{comm.comm_type}</td>
                    <td className="px-4 py-3">
                      <span
                        className={`px-2 py-0.5 rounded text-xs font-medium ${
                          comm.direction === "inbound"
                            ? "bg-blue-100 text-blue-700"
                            : "bg-gray-100 text-gray-600"
                        }`}
                      >
                        {comm.direction}
                      </span>
                    </td>
                    <td className="px-4 py-3 text-sm">{comm.subject}</td>
                    <td className="px-4 py-3 text-sm text-gray-500">
                      {comm.status}
                    </td>
                    <td className="px-4 py-3 text-sm text-gray-500">
                      {new Date(comm.created_at).toLocaleString()}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>
      </div>
    </div>
  );
}
