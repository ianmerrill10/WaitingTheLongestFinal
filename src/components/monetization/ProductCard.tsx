"use client";

interface ProductCardProps {
  product: {
    id: string;
    name: string;
    category: string;
    affiliate_url: string;
    image_url: string | null;
    price_min: number | null;
    price_max: number | null;
    description: string | null;
    partner_name?: string;
  };
  dogId?: string;
}

export default function ProductCard({ product, dogId }: ProductCardProps) {
  const handleClick = () => {
    // Fire click tracking asynchronously
    fetch("/api/affiliate/click", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        product_id: product.id,
        dog_id: dogId,
        page_url: window.location.href,
      }),
    }).catch(() => {});
  };

  const priceText =
    product.price_min && product.price_max
      ? `$${product.price_min} - $${product.price_max}`
      : product.price_min
        ? `From $${product.price_min}`
        : null;

  return (
    <a
      href={product.affiliate_url}
      target="_blank"
      rel="noopener noreferrer sponsored"
      onClick={handleClick}
      className="block bg-white rounded-lg border border-gray-200 hover:border-green-300 hover:shadow-sm transition-all overflow-hidden"
    >
      {product.image_url && (
        <div className="aspect-square bg-gray-100">
          <img
            src={product.image_url}
            alt={product.name}
            className="w-full h-full object-contain p-2"
            loading="lazy"
          />
        </div>
      )}
      <div className="p-3">
        <h3 className="text-sm font-medium text-gray-900 line-clamp-2">
          {product.name}
        </h3>
        {product.description && (
          <p className="text-xs text-gray-500 mt-1 line-clamp-2">
            {product.description}
          </p>
        )}
        <div className="flex items-center justify-between mt-2">
          {priceText && (
            <span className="text-sm font-semibold text-green-700">
              {priceText}
            </span>
          )}
          {product.partner_name && (
            <span className="text-xs text-gray-400">
              {product.partner_name}
            </span>
          )}
        </div>
      </div>
    </a>
  );
}
