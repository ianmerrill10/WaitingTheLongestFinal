export default function DogsLoading() {
  return (
    <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
      <div className="flex flex-col lg:flex-row gap-8">
        {/* Sidebar skeleton */}
        <aside className="w-full lg:w-64 flex-shrink-0">
          <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-5 animate-pulse">
            <div className="h-6 bg-gray-200 rounded w-1/2 mb-4" />
            {Array.from({ length: 5 }).map((_, i) => (
              <div key={i} className="mb-4">
                <div className="h-4 bg-gray-200 rounded w-1/3 mb-2" />
                <div className="h-9 bg-gray-100 rounded" />
              </div>
            ))}
          </div>
        </aside>
        {/* Content skeleton */}
        <div className="flex-1">
          <div className="h-5 bg-gray-200 rounded w-32 mb-6" />
          <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-4">
            {Array.from({ length: 6 }).map((_, i) => (
              <div key={i} className="bg-white rounded-lg shadow-sm border border-gray-200 overflow-hidden animate-pulse">
                <div className="aspect-square bg-gray-200" />
                <div className="p-4 space-y-2">
                  <div className="h-5 bg-gray-200 rounded w-3/4" />
                  <div className="h-4 bg-gray-200 rounded w-1/2" />
                  <div className="h-3 bg-gray-200 rounded w-1/3" />
                  <div className="h-8 bg-gray-900 rounded mt-3" />
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}
