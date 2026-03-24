"use client";

import { useState, useMemo } from "react";

type SortDirection = "asc" | "desc";

export interface Column {
  key: string;
  label: string;
  sortable?: boolean;
  align?: "left" | "right" | "center";
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  render?: (value: any, row: any) => React.ReactNode;
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  exportValue?: (value: any, row: any) => string;
}

interface SortableTableProps {
  columns: Column[];
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  data: any[];
  searchKeys?: string[];
  title?: string;
  exportFilename?: string;
  pageSize?: number;
  compact?: boolean;
  emptyMessage?: string;
}

export default function SortableTable({
  columns,
  data,
  searchKeys,
  title,
  exportFilename,
  pageSize = 25,
  compact = false,
  emptyMessage = "No data available",
}: SortableTableProps) {
  const [sortKey, setSortKey] = useState<string | null>(null);
  const [sortDir, setSortDir] = useState<SortDirection>("desc");
  const [search, setSearch] = useState("");
  const [page, setPage] = useState(0);

  // Filter
  const filtered = useMemo(() => {
    if (!search || !searchKeys) return data;
    const q = search.toLowerCase();
    return data.filter((row) =>
      searchKeys.some((key) => String(row[key] ?? "").toLowerCase().includes(q))
    );
  }, [data, search, searchKeys]);

  // Sort
  const sorted = useMemo(() => {
    if (!sortKey) return filtered;
    return [...filtered].sort((a, b) => {
      const aVal = a[sortKey];
      const bVal = b[sortKey];
      if (aVal == null && bVal == null) return 0;
      if (aVal == null) return 1;
      if (bVal == null) return -1;
      if (typeof aVal === "number" && typeof bVal === "number") {
        return sortDir === "asc" ? aVal - bVal : bVal - aVal;
      }
      return sortDir === "asc"
        ? String(aVal).localeCompare(String(bVal))
        : String(bVal).localeCompare(String(aVal));
    });
  }, [filtered, sortKey, sortDir]);

  // Paginate
  const totalPages = Math.ceil(sorted.length / pageSize);
  const paginated = sorted.slice(page * pageSize, (page + 1) * pageSize);

  const handleSort = (key: string) => {
    if (sortKey === key) {
      setSortDir((d) => (d === "asc" ? "desc" : "asc"));
    } else {
      setSortKey(key);
      setSortDir("desc");
    }
    setPage(0);
  };

  const exportCSV = () => {
    const headers = columns.map((c) => c.label).join(",");
    const rows = sorted.map((row) =>
      columns
        .map((col) => {
          const val = col.exportValue
            ? col.exportValue(row[col.key], row)
            : String(row[col.key] ?? "");
          return `"${val.replace(/"/g, '""')}"`;
        })
        .join(",")
    );
    const csv = [headers, ...rows].join("\n");
    const blob = new Blob([csv], { type: "text/csv" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `${exportFilename || "export"}.csv`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const alignClass = (align?: string) =>
    align === "right" ? "text-right" : align === "center" ? "text-center" : "text-left";

  return (
    <div className="bg-white rounded-lg border">
      {/* Header */}
      <div className="flex items-center justify-between p-4 border-b">
        <div className="flex items-center gap-3">
          {title && <h3 className="font-bold text-gray-900">{title}</h3>}
          <span className="text-xs text-gray-400 tabular-nums">{sorted.length} rows</span>
        </div>
        <div className="flex items-center gap-2">
          {searchKeys && (
            <input
              type="text"
              placeholder="Search..."
              value={search}
              onChange={(e) => {
                setSearch(e.target.value);
                setPage(0);
              }}
              className="px-3 py-1.5 text-sm border rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 w-48"
            />
          )}
          {exportFilename && (
            <button
              onClick={exportCSV}
              className="px-3 py-1.5 text-xs font-medium bg-gray-100 hover:bg-gray-200 rounded-lg transition"
            >
              Export CSV
            </button>
          )}
        </div>
      </div>

      {/* Table */}
      <div className="overflow-x-auto">
        <table className="w-full text-sm">
          <thead>
            <tr className="border-b bg-gray-50">
              {columns.map((col) => (
                <th
                  key={col.key}
                  className={`py-2.5 px-4 font-medium text-gray-500 ${alignClass(col.align)} ${
                    col.sortable !== false
                      ? "cursor-pointer hover:text-gray-900 select-none"
                      : ""
                  }`}
                  onClick={col.sortable !== false ? () => handleSort(col.key) : undefined}
                >
                  <span className="inline-flex items-center gap-1">
                    {col.label}
                    {col.sortable !== false && sortKey === col.key && (
                      <span className="text-green-600">
                        {sortDir === "asc" ? "\u2191" : "\u2193"}
                      </span>
                    )}
                  </span>
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {paginated.length === 0 ? (
              <tr>
                <td
                  colSpan={columns.length}
                  className="py-8 text-center text-gray-400"
                >
                  {emptyMessage}
                </td>
              </tr>
            ) : (
              paginated.map((row, i) => (
                <tr
                  key={i}
                  className="border-b border-gray-50 hover:bg-gray-50"
                >
                  {columns.map((col) => (
                    <td
                      key={col.key}
                      className={`${compact ? "py-1.5" : "py-2.5"} px-4 ${alignClass(col.align)} tabular-nums`}
                    >
                      {col.render
                        ? col.render(row[col.key], row)
                        : String(row[col.key] ?? "")}
                    </td>
                  ))}
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>

      {/* Pagination */}
      {totalPages > 1 && (
        <div className="flex items-center justify-between p-3 border-t text-xs text-gray-500">
          <span>
            {page * pageSize + 1}&ndash;
            {Math.min((page + 1) * pageSize, sorted.length)} of{" "}
            {sorted.length}
          </span>
          <div className="flex gap-1">
            <button
              onClick={() => setPage(0)}
              disabled={page === 0}
              className="px-2 py-1 rounded hover:bg-gray-100 disabled:opacity-30"
            >
              &laquo;&laquo;
            </button>
            <button
              onClick={() => setPage((p) => Math.max(0, p - 1))}
              disabled={page === 0}
              className="px-2 py-1 rounded hover:bg-gray-100 disabled:opacity-30"
            >
              &laquo;
            </button>
            <span className="px-2 py-1">
              Page {page + 1} of {totalPages}
            </span>
            <button
              onClick={() => setPage((p) => Math.min(totalPages - 1, p + 1))}
              disabled={page >= totalPages - 1}
              className="px-2 py-1 rounded hover:bg-gray-100 disabled:opacity-30"
            >
              &raquo;
            </button>
            <button
              onClick={() => setPage(totalPages - 1)}
              disabled={page >= totalPages - 1}
              className="px-2 py-1 rounded hover:bg-gray-100 disabled:opacity-30"
            >
              &raquo;&raquo;
            </button>
          </div>
        </div>
      )}
    </div>
  );
}
