"use client";

import { useState } from "react";

interface InterestFormProps {
  dogId: string;
  dogName: string;
  shelterId: string;
}

export default function InterestForm({
  dogId,
  dogName,
  shelterId,
}: InterestFormProps) {
  const [isOpen, setIsOpen] = useState(false);
  const [submitting, setSubmitting] = useState(false);
  const [submitted, setSubmitted] = useState(false);
  const [error, setError] = useState<string | null>(null);

  async function handleSubmit(e: React.FormEvent<HTMLFormElement>) {
    e.preventDefault();
    setSubmitting(true);
    setError(null);

    const form = e.currentTarget;
    const formData = new FormData(form);

    try {
      const res = await fetch(`/api/dogs/${dogId}/interest`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          name: formData.get("name"),
          email: formData.get("email"),
          phone: formData.get("phone"),
          message: formData.get("message"),
          shelter_id: shelterId,
        }),
      });

      if (!res.ok) throw new Error("Failed to submit");
      setSubmitted(true);
    } catch {
      setError("Something went wrong. Please try again or contact the shelter directly.");
    } finally {
      setSubmitting(false);
    }
  }

  if (submitted) {
    return (
      <div className="bg-green-50 rounded-xl p-6 border border-green-200 text-center">
        <div className="text-3xl mb-2">&#10003;</div>
        <h3 className="font-bold text-green-900 text-lg mb-1">
          Interest Submitted!
        </h3>
        <p className="text-sm text-green-700">
          The shelter has been notified about your interest in {dogName}.
          They&apos;ll reach out to you soon!
        </p>
      </div>
    );
  }

  if (!isOpen) {
    return (
      <div className="bg-white rounded-xl p-6 shadow-sm border border-gray-200">
        <button
          onClick={() => setIsOpen(true)}
          className="w-full py-4 px-6 bg-green-500 hover:bg-green-600 text-white text-lg font-bold rounded-lg transition shadow-lg shadow-green-500/25"
        >
          I&apos;m Interested in {dogName}!
        </button>
        <p className="text-xs text-gray-500 text-center mt-3">
          Fill out a quick form and the shelter will contact you
        </p>
      </div>
    );
  }

  return (
    <div className="bg-white rounded-xl p-6 shadow-sm border border-green-300">
      <h3 className="font-bold text-gray-900 mb-4">
        I&apos;m Interested in {dogName}!
      </h3>
      <form onSubmit={handleSubmit} className="space-y-3">
        <div>
          <label htmlFor="interest-name" className="block text-sm font-medium text-gray-700 mb-1">
            Your Name *
          </label>
          <input
            id="interest-name"
            name="name"
            type="text"
            required
            className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-green-500"
          />
        </div>
        <div>
          <label htmlFor="interest-email" className="block text-sm font-medium text-gray-700 mb-1">
            Email *
          </label>
          <input
            id="interest-email"
            name="email"
            type="email"
            required
            className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-green-500"
          />
        </div>
        <div>
          <label htmlFor="interest-phone" className="block text-sm font-medium text-gray-700 mb-1">
            Phone (optional)
          </label>
          <input
            id="interest-phone"
            name="phone"
            type="tel"
            className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-green-500"
          />
        </div>
        <div>
          <label htmlFor="interest-message" className="block text-sm font-medium text-gray-700 mb-1">
            Message (optional)
          </label>
          <textarea
            id="interest-message"
            name="message"
            rows={3}
            placeholder={`Tell the shelter why you'd be a great home for ${dogName}...`}
            className="w-full px-3 py-2 border border-gray-300 rounded-md text-sm focus:outline-none focus:ring-2 focus:ring-green-500"
          />
        </div>
        {error && (
          <p className="text-red-600 text-sm">{error}</p>
        )}
        <button
          type="submit"
          disabled={submitting}
          className="w-full py-3 px-6 bg-green-500 hover:bg-green-600 disabled:bg-green-300 text-white font-bold rounded-lg transition"
        >
          {submitting ? "Sending..." : `Send Interest for ${dogName}`}
        </button>
        <button
          type="button"
          onClick={() => setIsOpen(false)}
          className="w-full py-2 text-sm text-gray-500 hover:text-gray-700"
        >
          Cancel
        </button>
      </form>
    </div>
  );
}
