"use client";

import Link from "next/link";

interface ShelterRow {
  id: string;
  name: string;
  city: string;
  state_code: string;
  zip_code?: string | null;
  shelter_type?: string | null;
  is_kill_shelter?: boolean;
  accepts_rescue_pulls?: boolean;
  is_verified?: boolean;
  phone?: string | null;
  email?: string | null;
  website?: string | null;
  available_dog_count?: number;
  urgent_dog_count?: number;
  website_platform?: string | null;
  last_scraped_at?: string | null;
  ein?: string | null;
}

interface ShelterTableProps {
  shelters: ShelterRow[];
  sort: string;
  onSort: (sort: string) => void;
}

interface SortableColumn {
  key: string;
  label: string;
  altKey?: string;
}

const SORTABLE_COLUMNS: SortableColumn[] = [
  { key: "name_asc", label: "Name", altKey: "name_desc" },
  { key: "state", label: "State" },
  { key: "most_dogs", label: "Dogs" },
  { key: "most_urgent", label: "Urgent" },
];

export default function ShelterTable({ shelters, sort, onSort }: ShelterTableProps) {
  function handleHeaderClick(col: SortableColumn) {
    if (sort === col.key && col.altKey) {
      onSort(col.altKey);
    } else if (col.altKey && sort === col.altKey) {
      onSort(col.key);
    } else {
      onSort(col.key);
    }
  }

  function sortIndicator(col: SortableColumn) {
    if (sort === col.key) return " ▲";
    if (col.altKey && sort === col.altKey) return " ▼";
    return "";
  }

  if (shelters.length === 0) {
    return (
      <div className="text-center py-12 bg-gray-50 rounded-lg border border-dashed border-gray-300">
        <p className="text-gray-600 font-medium">No shelters found</p>
        <p className="text-sm text-gray-500 mt-1">Try adjusting your filters.</p>
      </div>
    );
  }

  return (
    <div className="overflow-x-auto rounded-lg border border-gray-200">
      <table className="w-full text-sm text-left">
        <thead className="bg-gray-50 text-gray-600 text-xs uppercase tracking-wide">
          <tr>
            {SORTABLE_COLUMNS.map((col) => (
              <th
                key={col.key}
                className="px-4 py-3 cursor-pointer hover:bg-gray-100 transition select-none whitespace-nowrap"
                onClick={() => handleHeaderClick(col)}
              >
                {col.label}{sortIndicator(col)}
              </th>
            ))}
            <th className="px-4 py-3 hidden md:table-cell">Type</th>
            <th className="px-4 py-3 hidden lg:table-cell">Kill?</th>
            <th className="px-4 py-3 hidden lg:table-cell">Rescue?</th>
            <th className="px-4 py-3 hidden md:table-cell">Phone</th>
            <th className="px-4 py-3 hidden xl:table-cell">Platform</th>
            <th className="px-4 py-3 hidden xl:table-cell">Last Scraped</th>
          </tr>
        </thead>
        <tbody className="divide-y divide-gray-100">
          {shelters.map((s) => (
            <tr key={s.id} className="hover:bg-gray-50 transition">
              <td className="px-4 py-3">
                <Link href={`/shelters/${s.id}`} className="font-medium text-blue-600 hover:underline">
                  {s.name}
                </Link>
                <p className="text-xs text-gray-400">{s.city}</p>
              </td>
              <td className="px-4 py-3 font-medium">{s.state_code}</td>
              <td className="px-4 py-3">
                <span className={`font-semibold ${(s.available_dog_count || 0) > 0 ? "text-gray-900" : "text-gray-400"}`}>
                  {s.available_dog_count || 0}
                </span>
              </td>
              <td className="px-4 py-3">
                {(s.urgent_dog_count || 0) > 0 ? (
                  <span className="text-red-600 font-semibold">{s.urgent_dog_count}</span>
                ) : (
                  <span className="text-gray-400">0</span>
                )}
              </td>
              <td className="px-4 py-3 hidden md:table-cell">
                <span className="text-xs capitalize text-gray-500">
                  {s.shelter_type?.replace(/_/g, " ") || "—"}
                </span>
              </td>
              <td className="px-4 py-3 hidden lg:table-cell">
                {s.is_kill_shelter ? (
                  <span className="text-red-500 text-xs font-medium">Yes</span>
                ) : (
                  <span className="text-gray-400 text-xs">No</span>
                )}
              </td>
              <td className="px-4 py-3 hidden lg:table-cell">
                {s.accepts_rescue_pulls ? (
                  <span className="text-green-600 text-xs font-medium">Yes</span>
                ) : (
                  <span className="text-gray-400 text-xs">No</span>
                )}
              </td>
              <td className="px-4 py-3 hidden md:table-cell text-xs text-gray-500">
                {s.phone ? (
                  <a href={`tel:${s.phone}`} className="hover:text-gray-700">{s.phone}</a>
                ) : "—"}
              </td>
              <td className="px-4 py-3 hidden xl:table-cell text-xs text-gray-400 capitalize">
                {s.website_platform?.replace(/_/g, " ") || "—"}
              </td>
              <td className="px-4 py-3 hidden xl:table-cell text-xs text-gray-400">
                {s.last_scraped_at
                  ? new Date(s.last_scraped_at).toLocaleDateString()
                  : "Never"}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
