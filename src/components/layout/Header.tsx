"use client";

import Link from "next/link";
import { useState } from "react";

export default function Header() {
  const [mobileMenuOpen, setMobileMenuOpen] = useState(false);

  return (
    <header className="bg-wtl-dark text-white sticky top-0 z-50 shadow-lg">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="flex items-center justify-between h-16">
          {/* Logo */}
          <Link href="/" className="flex items-center gap-2">
            <span className="text-2xl">🐕</span>
            <div>
              <span className="text-lg font-bold tracking-tight">
                Waiting<span className="text-led-green">The</span>Longest
              </span>
              <span className="hidden sm:inline text-xs text-gray-400 ml-2">
                Stop a Clock, Save a Life
              </span>
            </div>
          </Link>

          {/* Desktop Navigation */}
          <nav className="hidden md:flex items-center gap-6">
            <Link
              href="/dogs"
              className="text-gray-300 hover:text-white transition"
            >
              Browse Dogs
            </Link>
            <Link
              href="/urgent"
              className="text-red-400 hover:text-red-300 font-semibold transition flex items-center gap-1"
            >
              <span className="inline-block w-2 h-2 bg-red-500 rounded-full animate-pulse" />
              Urgent
            </Link>
            <Link
              href="/overlooked"
              className="text-gray-300 hover:text-white transition"
            >
              Overlooked Angels
            </Link>
            <Link
              href="/states"
              className="text-gray-300 hover:text-white transition"
            >
              By State
            </Link>
            <Link
              href="/breeds"
              className="text-gray-300 hover:text-white transition"
            >
              Breeds
            </Link>
            <Link
              href="/shelters"
              className="text-gray-300 hover:text-white transition"
            >
              Shelters
            </Link>
            <Link
              href="/foster"
              className="text-gray-300 hover:text-white transition"
            >
              Foster
            </Link>
            <Link
              href="/saved"
              className="text-gray-300 hover:text-white transition flex items-center gap-1"
            >
              <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                <path strokeLinecap="round" strokeLinejoin="round" d="M4.318 6.318a4.5 4.5 0 000 6.364L12 20.364l7.682-7.682a4.5 4.5 0 00-6.364-6.364L12 7.636l-1.318-1.318a4.5 4.5 0 00-6.364 0z" />
              </svg>
              Saved
            </Link>
          </nav>

          {/* Mobile menu button */}
          <button
            className="md:hidden p-2 rounded-md text-gray-400 hover:text-white"
            onClick={() => setMobileMenuOpen(!mobileMenuOpen)}
            aria-label="Toggle navigation menu"
            aria-expanded={mobileMenuOpen ? "true" : "false"}
          >
            <svg
              className="h-6 w-6"
              fill="none"
              viewBox="0 0 24 24"
              stroke="currentColor"
            >
              {mobileMenuOpen ? (
                <path
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  strokeWidth={2}
                  d="M6 18L18 6M6 6l12 12"
                />
              ) : (
                <path
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  strokeWidth={2}
                  d="M4 6h16M4 12h16M4 18h16"
                />
              )}
            </svg>
          </button>
        </div>
      </div>

      {/* Mobile Navigation */}
      {mobileMenuOpen && (
        <div className="md:hidden border-t border-gray-700">
          <div className="px-4 py-3 space-y-2">
            <Link
              href="/dogs"
              className="block px-3 py-2 rounded-md text-gray-300 hover:bg-gray-700"
              onClick={() => setMobileMenuOpen(false)}
            >
              Browse Dogs
            </Link>
            <Link
              href="/urgent"
              className="block px-3 py-2 rounded-md text-red-400 font-semibold hover:bg-gray-700"
              onClick={() => setMobileMenuOpen(false)}
            >
              🔴 Urgent Dogs
            </Link>
            <Link
              href="/overlooked"
              className="block px-3 py-2 rounded-md text-gray-300 hover:bg-gray-700"
              onClick={() => setMobileMenuOpen(false)}
            >
              Overlooked Angels
            </Link>
            <Link
              href="/states"
              className="block px-3 py-2 rounded-md text-gray-300 hover:bg-gray-700"
              onClick={() => setMobileMenuOpen(false)}
            >
              By State
            </Link>
            <Link
              href="/breeds"
              className="block px-3 py-2 rounded-md text-gray-300 hover:bg-gray-700"
              onClick={() => setMobileMenuOpen(false)}
            >
              Breeds
            </Link>
            <Link
              href="/shelters"
              className="block px-3 py-2 rounded-md text-gray-300 hover:bg-gray-700"
              onClick={() => setMobileMenuOpen(false)}
            >
              Shelters
            </Link>
            <Link
              href="/foster"
              className="block px-3 py-2 rounded-md text-gray-300 hover:bg-gray-700"
              onClick={() => setMobileMenuOpen(false)}
            >
              Foster a Dog
            </Link>
            <Link
              href="/saved"
              className="block px-3 py-2 rounded-md text-gray-300 hover:bg-gray-700"
              onClick={() => setMobileMenuOpen(false)}
            >
              Saved Dogs
            </Link>
          </div>
        </div>
      )}
    </header>
  );
}
