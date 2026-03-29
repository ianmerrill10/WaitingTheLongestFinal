/* eslint-disable @typescript-eslint/no-explicit-any */

export function generateDogJsonLd(dog: any, shelter: any) {
  return {
    "@context": "https://schema.org",
    "@type": "Thing",
    "name": dog.name,
    "description": dog.description || `${dog.name} is a ${dog.breed_primary} available for adoption.`,
    "image": dog.primary_photo_url || undefined,
    "url": `https://waitingthelongest.com/dogs/${dog.id}`,
    "additionalType": "https://schema.org/Animal",
    "provider": {
      "@type": "AnimalShelter",
      "name": shelter?.name || "Unknown Shelter",
      "address": shelter?.city && shelter?.state_code ? {
        "@type": "PostalAddress",
        "addressLocality": shelter.city,
        "addressRegion": shelter.state_code,
      } : undefined,
      "url": shelter?.website || undefined,
    },
    "additionalProperty": [
      { "@type": "PropertyValue", "name": "Breed", "value": dog.breed_primary },
      { "@type": "PropertyValue", "name": "Age", "value": dog.age_category },
      { "@type": "PropertyValue", "name": "Size", "value": dog.size },
      { "@type": "PropertyValue", "name": "Gender", "value": dog.gender },
      { "@type": "PropertyValue", "name": "Adoption Fee", "value": dog.is_fee_waived ? "Free" : (dog.adoption_fee ? `$${dog.adoption_fee}` : "Contact shelter") },
    ]
  };
}

export function generateShelterJsonLd(shelter: any) {
  return {
    "@context": "https://schema.org",
    "@type": "AnimalShelter",
    "name": shelter.name,
    "address": {
      "@type": "PostalAddress",
      "addressLocality": shelter.city,
      "addressRegion": shelter.state_code,
      "postalCode": shelter.zip_code || undefined,
    },
    "telephone": shelter.phone || undefined,
    "email": shelter.email || undefined,
    "url": shelter.website || `https://waitingthelongest.com/shelters/${shelter.id}`,
  };
}

export function generateOrganizationJsonLd() {
  return {
    "@context": "https://schema.org",
    "@type": "Organization",
    "name": "WaitingTheLongest.com",
    "url": "https://waitingthelongest.com",
    "description": "Find adoptable dogs from shelters and rescues across the United States. Real-time euthanasia countdown timers help you save dogs before time runs out.",
    "sameAs": [],
    "logo": "https://waitingthelongest.com/favicon.ico",
  };
}

export function generateBreadcrumbJsonLd(items: { name: string; url: string }[]) {
  return {
    "@context": "https://schema.org",
    "@type": "BreadcrumbList",
    "itemListElement": items.map((item, i) => ({
      "@type": "ListItem",
      "position": i + 1,
      "name": item.name,
      "item": item.url,
    })),
  };
}
