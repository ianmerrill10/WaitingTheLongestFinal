"use client";

import { useState } from "react";

const CHECKLIST_ITEMS = [
  { id: "crate", label: "Crate or kennel (appropriately sized)", category: "Essential" },
  { id: "food", label: "Dog food (ask shelter for brand recommendations)", category: "Essential" },
  { id: "bowls", label: "Food and water bowls", category: "Essential" },
  { id: "leash", label: "Leash and collar", category: "Essential" },
  { id: "bed", label: "Dog bed or blankets", category: "Essential" },
  { id: "poop_bags", label: "Poop bags", category: "Essential" },
  { id: "id_tag", label: "ID tag with your contact info", category: "Essential" },
  { id: "toys", label: "Chew toys and enrichment toys", category: "Comfort" },
  { id: "treats", label: "Training treats", category: "Comfort" },
  { id: "gate", label: "Baby gate (for room separation)", category: "Comfort" },
  { id: "enzyme", label: "Enzyme cleaner (for accidents)", category: "Comfort" },
  { id: "brush", label: "Grooming brush", category: "Comfort" },
  { id: "vet_info", label: "Shelter vet contact info saved", category: "Preparation" },
  { id: "safe_room", label: "Quiet decompression room prepared", category: "Preparation" },
  { id: "household", label: "All household members informed", category: "Preparation" },
  { id: "other_pets", label: "Introduction plan for existing pets", category: "Preparation" },
  { id: "schedule", label: "Feeding and walking schedule set", category: "Preparation" },
];

export default function FosterChecklist() {
  const [checked, setChecked] = useState<Record<string, boolean>>({});

  function toggle(id: string) {
    setChecked(prev => ({ ...prev, [id]: !prev[id] }));
  }

  const total = CHECKLIST_ITEMS.length;
  const done = Object.values(checked).filter(Boolean).length;
  const pct = Math.round((done / total) * 100);

  const categories = ["Essential", "Comfort", "Preparation"];

  return (
    <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
      <div className="flex items-center justify-between mb-4">
        <h3 className="font-bold text-gray-900 text-lg">Foster Readiness Checklist</h3>
        <span className="text-sm font-medium text-gray-500">{done}/{total}</span>
      </div>

      {/* Progress bar */}
      <div className="w-full bg-gray-100 rounded-full h-2 mb-6">
        <div className="bg-green-500 h-2 rounded-full transition-all duration-300" style={{ width: `${pct}%` }} />
      </div>

      {categories.map(cat => (
        <div key={cat} className="mb-4">
          <h4 className="text-sm font-semibold text-gray-700 mb-2 uppercase tracking-wide">{cat}</h4>
          <div className="space-y-1.5">
            {CHECKLIST_ITEMS.filter(item => item.category === cat).map(item => (
              <label key={item.id} className="flex items-start gap-2.5 cursor-pointer group py-1">
                <input
                  type="checkbox"
                  checked={checked[item.id] || false}
                  onChange={() => toggle(item.id)}
                  className="mt-0.5 rounded border-gray-300 text-green-600 focus:ring-green-500"
                />
                <span className={`text-sm transition ${checked[item.id] ? "text-gray-400 line-through" : "text-gray-700 group-hover:text-gray-900"}`}>
                  {item.label}
                </span>
              </label>
            ))}
          </div>
        </div>
      ))}

      {pct === 100 && (
        <div className="mt-4 p-3 bg-green-50 border border-green-200 rounded-lg text-center">
          <p className="text-green-700 font-medium text-sm">You&apos;re ready to foster!</p>
        </div>
      )}
    </div>
  );
}
