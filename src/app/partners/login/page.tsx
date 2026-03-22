"use client";

import { useState } from "react";
import Link from "next/link";

export default function PartnerLoginPage() {
  const [email, setEmail] = useState("");
  const [apiKey, setApiKey] = useState("");
  const [loginMethod, setLoginMethod] = useState<"key" | "magic">("key");
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError(null);
    setSuccess(null);
    setLoading(true);

    try {
      const body: Record<string, string> = { email };
      if (loginMethod === "key") body.api_key = apiKey;

      const res = await fetch("/api/partners/login", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(body),
      });

      const data = await res.json();

      if (!res.ok) {
        setError(data.error || "Login failed");
        return;
      }

      if (loginMethod === "key" && data.success) {
        window.location.href = "/partners/dashboard";
      } else {
        setSuccess(data.message);
      }
    } catch {
      setError("Network error. Please try again.");
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="min-h-screen bg-gray-50 flex items-center justify-center py-12 px-4">
      <div className="max-w-md w-full">
        <div className="text-center mb-8">
          <h1 className="text-3xl font-bold mb-2">Shelter Partner Login</h1>
          <p className="text-gray-600">
            Access your shelter dashboard to manage dogs, API keys, and
            integrations.
          </p>
        </div>

        <div className="bg-white rounded-xl shadow-sm border border-gray-200 p-6">
          {/* Method Toggle */}
          <div className="flex border border-gray-200 rounded-lg mb-6 overflow-hidden">
            <button
              onClick={() => setLoginMethod("key")}
              className={`flex-1 py-2 text-sm font-medium transition-colors ${
                loginMethod === "key"
                  ? "bg-green-700 text-white"
                  : "bg-white text-gray-600 hover:bg-gray-50"
              }`}
            >
              API Key Login
            </button>
            <button
              onClick={() => setLoginMethod("magic")}
              className={`flex-1 py-2 text-sm font-medium transition-colors ${
                loginMethod === "magic"
                  ? "bg-green-700 text-white"
                  : "bg-white text-gray-600 hover:bg-gray-50"
              }`}
            >
              Magic Link
            </button>
          </div>

          <form onSubmit={handleSubmit} className="space-y-4">
            <div>
              <label className="block text-sm font-medium text-gray-700 mb-1">
                Email Address
              </label>
              <input
                type="email"
                value={email}
                onChange={(e) => setEmail(e.target.value)}
                required
                placeholder="you@yourshelter.org"
                className="form-input"
              />
            </div>

            {loginMethod === "key" && (
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">
                  API Key
                </label>
                <input
                  type="password"
                  value={apiKey}
                  onChange={(e) => setApiKey(e.target.value)}
                  required
                  placeholder="wtl_sk_..."
                  className="form-input font-mono text-sm"
                />
                <p className="text-xs text-gray-500 mt-1">
                  Your API key was provided when you registered. Contact us if
                  you need a new one.
                </p>
              </div>
            )}

            {loginMethod === "magic" && (
              <p className="text-sm text-gray-600 bg-gray-50 p-3 rounded-lg">
                We&apos;ll send a login link to your email. The link expires in 15
                minutes.
              </p>
            )}

            {error && (
              <div className="bg-red-50 border border-red-200 text-red-700 rounded-lg p-3 text-sm">
                {error}
              </div>
            )}

            {success && (
              <div className="bg-green-50 border border-green-200 text-green-700 rounded-lg p-3 text-sm">
                {success}
              </div>
            )}

            <button
              type="submit"
              disabled={loading}
              className="w-full py-2.5 bg-green-700 text-white rounded-lg font-semibold hover:bg-green-800 transition-colors disabled:opacity-50"
            >
              {loading
                ? "Signing in..."
                : loginMethod === "key"
                  ? "Sign In"
                  : "Send Login Link"}
            </button>
          </form>

          <div className="mt-6 pt-4 border-t border-gray-200 text-center">
            <p className="text-sm text-gray-600">
              Not a partner yet?{" "}
              <Link
                href="/partners/register"
                className="text-green-700 font-semibold hover:underline"
              >
                Register for free
              </Link>
            </p>
          </div>
        </div>
      </div>
    </div>
  );
}
