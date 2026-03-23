import Link from "next/link";

interface ShelterCardProps {
  shelter: {
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
  };
}

const TYPE_COLORS: Record<string, string> = {
  municipal: "bg-purple-50 text-purple-700",
  private: "bg-gray-100 text-gray-600",
  rescue: "bg-blue-50 text-blue-700",
  foster_network: "bg-teal-50 text-teal-700",
  humane_society: "bg-amber-50 text-amber-700",
  spca: "bg-indigo-50 text-indigo-700",
};

export default function ShelterCard({ shelter }: ShelterCardProps) {
  const dogCount = shelter.available_dog_count || 0;
  const urgentCount = shelter.urgent_dog_count || 0;

  return (
    <div className="bg-white rounded-lg border border-gray-200 p-5 hover:shadow-md transition">
      {/* Header */}
      <div className="flex items-start justify-between mb-2">
        <div className="min-w-0 flex-1">
          <h3 className="font-bold text-gray-900 truncate">{shelter.name}</h3>
          <p className="text-sm text-gray-500">
            {shelter.city}, {shelter.state_code}
            {shelter.zip_code ? ` ${shelter.zip_code}` : ""}
          </p>
        </div>
        {shelter.is_verified && (
          <span className="flex-shrink-0 ml-2 text-blue-500" title="Verified">
            <svg className="w-5 h-5" fill="currentColor" viewBox="0 0 20 20">
              <path fillRule="evenodd" d="M6.267 3.455a3.066 3.066 0 001.745-.723 3.066 3.066 0 013.976 0 3.066 3.066 0 001.745.723 3.066 3.066 0 012.812 2.812c.051.643.304 1.254.723 1.745a3.066 3.066 0 010 3.976 3.066 3.066 0 00-.723 1.745 3.066 3.066 0 01-2.812 2.812 3.066 3.066 0 00-1.745.723 3.066 3.066 0 01-3.976 0 3.066 3.066 0 00-1.745-.723 3.066 3.066 0 01-2.812-2.812 3.066 3.066 0 00-.723-1.745 3.066 3.066 0 010-3.976 3.066 3.066 0 00.723-1.745 3.066 3.066 0 012.812-2.812zm7.44 5.252a1 1 0 00-1.414-1.414L9 10.586 7.707 9.293a1 1 0 00-1.414 1.414l2 2a1 1 0 001.414 0l4-4z" clipRule="evenodd" />
            </svg>
          </span>
        )}
      </div>

      {/* Badges */}
      <div className="flex flex-wrap items-center gap-1.5 text-xs mb-3">
        {shelter.shelter_type && (
          <span className={`px-2 py-0.5 rounded-full capitalize ${TYPE_COLORS[shelter.shelter_type] || "bg-gray-100 text-gray-600"}`}>
            {shelter.shelter_type.replace(/_/g, " ")}
          </span>
        )}
        {shelter.is_kill_shelter && (
          <span className="px-2 py-0.5 bg-red-50 text-red-600 rounded-full font-medium">
            Kill Shelter
          </span>
        )}
        {shelter.accepts_rescue_pulls && (
          <span className="px-2 py-0.5 bg-green-50 text-green-600 rounded-full">
            Rescue Pulls
          </span>
        )}
        {shelter.website_platform && shelter.website_platform !== "unknown" && (
          <span className="px-2 py-0.5 bg-gray-50 text-gray-500 rounded-full">
            {shelter.website_platform.replace(/_/g, " ")}
          </span>
        )}
      </div>

      {/* Dog counts */}
      <div className="flex items-center gap-3 mb-3 text-sm">
        <span className="font-semibold text-gray-900">
          {dogCount} {dogCount === 1 ? "dog" : "dogs"}
        </span>
        {urgentCount > 0 && (
          <span className="text-red-600 font-medium">
            ({urgentCount} urgent)
          </span>
        )}
      </div>

      {/* Contact row */}
      <div className="flex items-center gap-3 text-xs text-gray-500 mb-4">
        {shelter.phone && (
          <a href={`tel:${shelter.phone}`} className="hover:text-gray-700 truncate">
            {shelter.phone}
          </a>
        )}
        {shelter.website && (
          <a
            href={shelter.website}
            target="_blank"
            rel="noopener noreferrer"
            className="hover:text-blue-600 truncate"
          >
            Website
          </a>
        )}
        {shelter.email && (
          <a href={`mailto:${shelter.email}`} className="hover:text-blue-600 truncate">
            Email
          </a>
        )}
      </div>

      {/* Last scraped */}
      {shelter.last_scraped_at && (
        <p className="text-xs text-gray-400 mb-3">
          Last checked: {new Date(shelter.last_scraped_at).toLocaleDateString()}
        </p>
      )}

      <Link
        href={`/shelters/${shelter.id}`}
        className="block w-full text-center py-2 px-4 bg-gray-50 hover:bg-gray-100 text-gray-700 font-medium rounded-md text-sm transition border border-gray-200"
      >
        View Shelter
      </Link>
    </div>
  );
}
