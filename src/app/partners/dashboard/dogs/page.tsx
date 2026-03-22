"use client";

import { useState, useEffect } from "react";
import Link from "next/link";

export default function DashboardDogsPage() {
  const [dogs, setDogs] = useState<Array<{
    id: string;
    name: string;
    breed: string;
    status: string;
    views: number;
    listed_date: string;
  }>>([]);

  useEffect(() => {
    const shelterId = document.cookie
      .split("; ")
      .find((c) => c.startsWith("wtl_shelter_id="))
      ?.split("=")[1];
    if (!shelterId) return;
    fetch(`/api/admin/partner?shelter_id=${shelterId}`)
      .then((r) => r.ok ? r.json() : null)
      .then((data) => { if (data?.dogs) setDogs(data.dogs); })
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
            <h1 className="text-2xl font-bold">Dog Management</h1>
          </div>
          <div className="flex gap-3">
            <button className="px-4 py-2 bg-white border border-gray-300 rounded-lg text-sm hover:bg-gray-50">
              Import CSV
            </button>
            <button className="px-4 py-2 bg-green-700 text-white rounded-lg text-sm hover:bg-green-800">
              Add Dog
            </button>
          </div>
        </div>

        {/* Filters */}
        <div className="bg-white rounded-lg border border-gray-200 p-4 mb-6 flex gap-4 flex-wrap">
          <select className="form-input w-auto">
            <option value="">All Statuses</option>
            <option value="available">Available</option>
            <option value="adopted">Adopted</option>
            <option value="hold">On Hold</option>
            <option value="pending">Pending</option>
          </select>
          <input
            type="text"
            placeholder="Search by name or breed..."
            className="form-input flex-1 min-w-[200px]"
          />
        </div>

        {/* Dogs Table */}
        <div className="bg-white rounded-lg border border-gray-200 overflow-hidden">
          {dogs.length === 0 ? (
            <div className="p-12 text-center text-gray-500">
              <p className="text-lg mb-2">No dogs listed yet</p>
              <p className="text-sm">
                Add your first dog using the button above, or import a CSV file
                to get started.
              </p>
            </div>
          ) : (
            <table className="w-full">
              <thead className="bg-gray-50 border-b border-gray-200">
                <tr>
                  <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">Name</th>
                  <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">Breed</th>
                  <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">Status</th>
                  <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">Views</th>
                  <th className="text-left px-4 py-3 text-sm font-medium text-gray-600">Listed</th>
                  <th className="text-right px-4 py-3 text-sm font-medium text-gray-600">Actions</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-gray-100">
                {dogs.map((dog) => (
                  <tr key={dog.id} className="hover:bg-gray-50">
                    <td className="px-4 py-3 font-medium">{dog.name}</td>
                    <td className="px-4 py-3 text-gray-600">{dog.breed}</td>
                    <td className="px-4 py-3">
                      <span className={`px-2 py-0.5 rounded-full text-xs font-medium ${
                        dog.status === "available"
                          ? "bg-green-100 text-green-700"
                          : dog.status === "adopted"
                            ? "bg-blue-100 text-blue-700"
                            : "bg-yellow-100 text-yellow-700"
                      }`}>
                        {dog.status}
                      </span>
                    </td>
                    <td className="px-4 py-3 text-gray-600">{dog.views}</td>
                    <td className="px-4 py-3 text-gray-600 text-sm">{dog.listed_date}</td>
                    <td className="px-4 py-3 text-right">
                      <button className="text-sm text-green-700 hover:underline mr-3">Edit</button>
                      <button className="text-sm text-red-600 hover:underline">Remove</button>
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
