/**
 * Seed US states into Supabase.
 * Run: npx tsx data/seed-states.ts
 */
import { createClient } from "@supabase/supabase-js";
import * as dotenv from "dotenv";
dotenv.config({ path: ".env.local" });

const supabase = createClient(
  process.env.NEXT_PUBLIC_SUPABASE_URL!,
  process.env.SUPABASE_SERVICE_ROLE_KEY!
);

const STATES = [
  { code: "AL", name: "Alabama", region: "Southeast" },
  { code: "AK", name: "Alaska", region: "West" },
  { code: "AZ", name: "Arizona", region: "Southwest" },
  { code: "AR", name: "Arkansas", region: "Southeast" },
  { code: "CA", name: "California", region: "West" },
  { code: "CO", name: "Colorado", region: "West" },
  { code: "CT", name: "Connecticut", region: "Northeast" },
  { code: "DE", name: "Delaware", region: "Northeast" },
  { code: "FL", name: "Florida", region: "Southeast" },
  { code: "GA", name: "Georgia", region: "Southeast" },
  { code: "HI", name: "Hawaii", region: "West" },
  { code: "ID", name: "Idaho", region: "West" },
  { code: "IL", name: "Illinois", region: "Midwest" },
  { code: "IN", name: "Indiana", region: "Midwest" },
  { code: "IA", name: "Iowa", region: "Midwest" },
  { code: "KS", name: "Kansas", region: "Midwest" },
  { code: "KY", name: "Kentucky", region: "Southeast" },
  { code: "LA", name: "Louisiana", region: "Southeast" },
  { code: "ME", name: "Maine", region: "Northeast" },
  { code: "MD", name: "Maryland", region: "Northeast" },
  { code: "MA", name: "Massachusetts", region: "Northeast" },
  { code: "MI", name: "Michigan", region: "Midwest" },
  { code: "MN", name: "Minnesota", region: "Midwest" },
  { code: "MS", name: "Mississippi", region: "Southeast" },
  { code: "MO", name: "Missouri", region: "Midwest" },
  { code: "MT", name: "Montana", region: "West" },
  { code: "NE", name: "Nebraska", region: "Midwest" },
  { code: "NV", name: "Nevada", region: "West" },
  { code: "NH", name: "New Hampshire", region: "Northeast" },
  { code: "NJ", name: "New Jersey", region: "Northeast" },
  { code: "NM", name: "New Mexico", region: "Southwest" },
  { code: "NY", name: "New York", region: "Northeast" },
  { code: "NC", name: "North Carolina", region: "Southeast" },
  { code: "ND", name: "North Dakota", region: "Midwest" },
  { code: "OH", name: "Ohio", region: "Midwest" },
  { code: "OK", name: "Oklahoma", region: "Southwest" },
  { code: "OR", name: "Oregon", region: "West" },
  { code: "PA", name: "Pennsylvania", region: "Northeast" },
  { code: "RI", name: "Rhode Island", region: "Northeast" },
  { code: "SC", name: "South Carolina", region: "Southeast" },
  { code: "SD", name: "South Dakota", region: "Midwest" },
  { code: "TN", name: "Tennessee", region: "Southeast" },
  { code: "TX", name: "Texas", region: "Southwest" },
  { code: "UT", name: "Utah", region: "West" },
  { code: "VT", name: "Vermont", region: "Northeast" },
  { code: "VA", name: "Virginia", region: "Southeast" },
  { code: "WA", name: "Washington", region: "West" },
  { code: "WV", name: "West Virginia", region: "Southeast" },
  { code: "WI", name: "Wisconsin", region: "Midwest" },
  { code: "WY", name: "Wyoming", region: "West" },
];

async function main() {
  console.log("Seeding 50 US states...");

  const { error } = await supabase.from("states").upsert(
    STATES.map((s) => ({
      code: s.code,
      name: s.name,
      region: s.region,
    })),
    { onConflict: "code" }
  );

  if (error) {
    console.error("Error seeding states:", error.message);
    process.exit(1);
  }

  console.log("Successfully seeded 50 states.");
}

main();
