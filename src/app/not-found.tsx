import Link from "next/link";

export default function NotFound() {
  return (
    <div className="max-w-2xl mx-auto px-4 py-20 text-center">
      <div className="text-8xl mb-6">🐕</div>
      <h1 className="text-4xl font-bold text-gray-900 mb-4">Page Not Found</h1>
      <p className="text-gray-500 text-lg mb-8">
        Looks like this page has found its forever home elsewhere.
      </p>
      <div className="flex flex-col sm:flex-row gap-4 justify-center">
        <Link href="/" className="px-6 py-3 bg-blue-600 text-white font-medium rounded-lg hover:bg-blue-700 transition">
          Go Home
        </Link>
        <Link href="/dogs" className="px-6 py-3 bg-gray-100 text-gray-700 font-medium rounded-lg hover:bg-gray-200 transition">
          Browse Dogs
        </Link>
        <Link href="/urgent" className="px-6 py-3 bg-red-600 text-white font-medium rounded-lg hover:bg-red-700 transition">
          Urgent Dogs
        </Link>
      </div>
    </div>
  );
}
