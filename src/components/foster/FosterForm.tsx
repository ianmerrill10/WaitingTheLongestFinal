"use client";

import { useState } from "react";

const HOUSING_OPTIONS = [
  "House with yard",
  "House without yard",
  "Apartment/Condo",
  "Townhouse",
  "Other",
];

const EXPERIENCE_OPTIONS = [
  "First-time dog owner",
  "Some experience",
  "Experienced dog owner",
  "Professional / trainer",
];

const SIZE_OPTIONS = [
  "Any size",
  "Small (under 25 lbs)",
  "Medium (25-50 lbs)",
  "Large (50-90 lbs)",
  "Extra Large (90+ lbs)",
];

export default function FosterForm() {
  const [firstName, setFirstName] = useState("");
  const [lastName, setLastName] = useState("");
  const [email, setEmail] = useState("");
  const [zipCode, setZipCode] = useState("");
  const [housingType, setHousingType] = useState("");
  const [dogExperience, setDogExperience] = useState("");
  const [preferredSize, setPreferredSize] = useState("Any size");
  const [notes, setNotes] = useState("");
  const [submitting, setSubmitting] = useState(false);
  const [success, setSuccess] = useState(false);
  const [error, setError] = useState("");

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError("");
    setSubmitting(true);

    try {
      const res = await fetch("/api/foster", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          firstName: firstName.trim(),
          lastName: lastName.trim(),
          email: email.trim(),
          zipCode: zipCode.trim(),
          housingType,
          dogExperience,
          preferredSize: preferredSize === "Any size" ? "any" : preferredSize,
          notes: notes.trim() || null,
        }),
      });

      const data = await res.json();

      if (!res.ok) {
        setError(data.error || "Something went wrong. Please try again.");
        return;
      }

      setSuccess(true);
    } catch {
      setError("Network error. Please check your connection and try again.");
    } finally {
      setSubmitting(false);
    }
  };

  if (success) {
    return (
      <div className="max-w-2xl mx-auto bg-white rounded-2xl shadow-lg border border-gray-200 overflow-hidden">
        <div className="bg-green-600 text-white p-6">
          <h2 className="text-2xl font-bold">Application Received!</h2>
        </div>
        <div className="p-8 text-center">
          <div className="w-16 h-16 bg-green-100 rounded-full flex items-center justify-center mx-auto mb-4">
            <svg className="w-8 h-8 text-green-600" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
              <path strokeLinecap="round" strokeLinejoin="round" d="M5 13l4 4L19 7" />
            </svg>
          </div>
          <h3 className="text-xl font-bold text-gray-900 mb-2">
            Thank you, {firstName}!
          </h3>
          <p className="text-gray-600 mb-4">
            Your foster application has been submitted successfully. We&apos;ll match you with shelters near zip code <strong>{zipCode}</strong> and reach out soon.
          </p>
          <p className="text-sm text-gray-500">
            In the meantime, check out the dogs who need help most urgently.
          </p>
          <a
            href="/urgent"
            className="inline-block mt-4 px-6 py-2 bg-red-600 text-white font-bold rounded-lg hover:bg-red-700 transition"
          >
            View Urgent Dogs
          </a>
        </div>
      </div>
    );
  }

  return (
    <div className="max-w-2xl mx-auto bg-white rounded-2xl shadow-lg border border-gray-200 overflow-hidden">
      <div className="bg-green-600 text-white p-6">
        <h2 className="text-2xl font-bold">Foster Application</h2>
        <p className="text-green-100 mt-1">
          Fill out this form and we will connect you with a local shelter.
        </p>
      </div>

      <form onSubmit={handleSubmit} className="p-6 space-y-5">
        {error && (
          <div className="bg-red-50 border border-red-200 text-red-700 px-4 py-3 rounded-lg text-sm">
            {error}
          </div>
        )}

        <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
          <div>
            <label htmlFor="first-name" className="block text-sm font-medium text-gray-700 mb-1">
              First Name *
            </label>
            <input
              id="first-name"
              type="text"
              required
              value={firstName}
              onChange={(e) => setFirstName(e.target.value)}
              className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 focus:border-transparent"
              placeholder="Jane"
            />
          </div>
          <div>
            <label htmlFor="last-name" className="block text-sm font-medium text-gray-700 mb-1">
              Last Name *
            </label>
            <input
              id="last-name"
              type="text"
              required
              value={lastName}
              onChange={(e) => setLastName(e.target.value)}
              className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 focus:border-transparent"
              placeholder="Smith"
            />
          </div>
        </div>

        <div>
          <label htmlFor="email" className="block text-sm font-medium text-gray-700 mb-1">
            Email Address *
          </label>
          <input
            id="email"
            type="email"
            required
            value={email}
            onChange={(e) => setEmail(e.target.value)}
            className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 focus:border-transparent"
            placeholder="jane@example.com"
          />
        </div>

        <div>
          <label htmlFor="zip" className="block text-sm font-medium text-gray-700 mb-1">
            Zip Code *
          </label>
          <input
            id="zip"
            type="text"
            required
            pattern="\d{5}(-\d{4})?"
            maxLength={10}
            value={zipCode}
            onChange={(e) => setZipCode(e.target.value)}
            className="w-full max-w-[200px] px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 focus:border-transparent"
            placeholder="90210"
          />
        </div>

        <div>
          <label htmlFor="housing" className="block text-sm font-medium text-gray-700 mb-1">
            Housing Type *
          </label>
          <select
            id="housing"
            required
            value={housingType}
            onChange={(e) => setHousingType(e.target.value)}
            className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 focus:border-transparent"
          >
            <option value="">Select...</option>
            {HOUSING_OPTIONS.map((opt) => (
              <option key={opt} value={opt}>{opt}</option>
            ))}
          </select>
        </div>

        <div>
          <label htmlFor="experience" className="block text-sm font-medium text-gray-700 mb-1">
            Dog Experience *
          </label>
          <select
            id="experience"
            required
            value={dogExperience}
            onChange={(e) => setDogExperience(e.target.value)}
            className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 focus:border-transparent"
          >
            <option value="">Select...</option>
            {EXPERIENCE_OPTIONS.map((opt) => (
              <option key={opt} value={opt}>{opt}</option>
            ))}
          </select>
        </div>

        <div>
          <label htmlFor="size-pref" className="block text-sm font-medium text-gray-700 mb-1">
            Preferred Dog Size
          </label>
          <select
            id="size-pref"
            value={preferredSize}
            onChange={(e) => setPreferredSize(e.target.value)}
            className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 focus:border-transparent"
          >
            {SIZE_OPTIONS.map((opt) => (
              <option key={opt} value={opt}>{opt}</option>
            ))}
          </select>
        </div>

        <div>
          <label htmlFor="notes" className="block text-sm font-medium text-gray-700 mb-1">
            Anything Else We Should Know?
          </label>
          <textarea
            id="notes"
            rows={3}
            maxLength={2000}
            value={notes}
            onChange={(e) => setNotes(e.target.value)}
            className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 focus:border-transparent"
            placeholder="Tell us about your home, other pets, schedule, etc."
          />
        </div>

        <div className="pt-2">
          <button
            type="submit"
            disabled={submitting}
            className={`w-full py-3 px-6 font-bold rounded-lg transition ${
              submitting
                ? "bg-green-400 cursor-not-allowed"
                : "bg-green-600 hover:bg-green-700 cursor-pointer"
            } text-white`}
          >
            {submitting ? "Submitting..." : "Submit Application"}
          </button>
          <p className="text-xs text-gray-500 text-center mt-3">
            Your information will be shared with local shelters in your area to match you with a foster dog.
          </p>
        </div>
      </form>
    </div>
  );
}
