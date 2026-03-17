import Link from "next/link";

export default function Footer() {
  return (
    <footer className="bg-wtl-dark text-gray-400 mt-auto">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-12">
        <div className="grid grid-cols-1 md:grid-cols-4 gap-8">
          {/* Brand */}
          <div>
            <div className="flex items-center gap-2 mb-3">
              <span className="text-2xl">🐕</span>
              <span className="text-lg font-bold text-white">
                WaitingTheLongest
              </span>
            </div>
            <p className="text-sm">
              Because Every Day Matters. Helping shelter dogs find loving homes
              since 2024.
            </p>
          </div>

          {/* Browse */}
          <div>
            <h3 className="text-white font-semibold mb-3">Browse</h3>
            <ul className="space-y-2 text-sm">
              <li>
                <Link href="/dogs" className="hover:text-white transition">
                  All Dogs
                </Link>
              </li>
              <li>
                <Link
                  href="/urgent"
                  className="hover:text-red-400 transition text-red-500"
                >
                  Urgent Dogs
                </Link>
              </li>
              <li>
                <Link
                  href="/overlooked"
                  className="hover:text-white transition"
                >
                  Overlooked Angels
                </Link>
              </li>
              <li>
                <Link href="/states" className="hover:text-white transition">
                  Browse by State
                </Link>
              </li>
              <li>
                <Link href="/shelters" className="hover:text-white transition">
                  Shelter Directory
                </Link>
              </li>
            </ul>
          </div>

          {/* Get Involved */}
          <div>
            <h3 className="text-white font-semibold mb-3">Get Involved</h3>
            <ul className="space-y-2 text-sm">
              <li>
                <Link href="/foster" className="hover:text-white transition">
                  Become a Foster
                </Link>
              </li>
              <li>
                <Link href="/quiz" className="hover:text-white transition">
                  Find Your Match
                </Link>
              </li>
              <li>
                <Link href="/stories" className="hover:text-white transition">
                  Success Stories
                </Link>
              </li>
            </ul>
          </div>

          {/* For Shelters */}
          <div>
            <h3 className="text-white font-semibold mb-3">For Shelters</h3>
            <ul className="space-y-2 text-sm">
              <li>
                <span className="text-green-400">Free to list</span> - always
              </li>
              <li>
                <Link href="/about" className="hover:text-white transition">
                  About Us
                </Link>
              </li>
            </ul>
          </div>
        </div>

        <div className="border-t border-gray-700 mt-8 pt-8 text-sm text-center">
          <p>
            &copy; {new Date().getFullYear()} WaitingTheLongest.com. All
            services free for shelters and rescues.
          </p>
        </div>
      </div>
    </footer>
  );
}
