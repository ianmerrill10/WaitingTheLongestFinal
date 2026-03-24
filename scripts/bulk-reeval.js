// One-time bulk re-evaluation: apply new stricter MAX_WAIT_DAYS to all dogs
const { createClient } = require("@supabase/supabase-js");
require("dotenv").config({ path: require("path").join(__dirname, "..", ".env.local") });

const MAX_WAIT_DAYS = {
  verified: 2555, // 7 years
  high: 1825,     // 5 years
  medium: 1095,   // 3 years
  low: 365,       // 1 year
  unknown: 180,   // 6 months
};

async function run() {
  const supabase = createClient(
    process.env.NEXT_PUBLIC_SUPABASE_URL,
    process.env.SUPABASE_SERVICE_ROLE_KEY,
    { auth: { autoRefreshToken: false, persistSession: false } }
  );

  const now = new Date();
  let totalCapped = 0;
  let totalDowngraded = 0;

  console.log("=== Bulk Re-evaluation: Applying stricter MAX_WAIT_DAYS ===\n");

  // Step 1: Cap wait times by confidence level
  for (const [conf, maxDays] of Object.entries(MAX_WAIT_DAYS)) {
    const cutoff = new Date(now.getTime() - maxDays * 24 * 60 * 60 * 1000);
    const cappedDate = cutoff.toISOString();

    // Paginate through all dogs exceeding the cap (Supabase default limit = 1000)
    let confCapped = 0;
    let offset = 0;
    while (true) {
      const { data: stale, error } = await supabase
        .from("dogs")
        .select("id, name, intake_date, date_source")
        .eq("is_available", true)
        .eq("date_confidence", conf)
        .lt("intake_date", cappedDate)
        .order("id")
        .range(offset, offset + 999);

      if (error) { console.log(`  Error for ${conf}: ${error.message}`); break; }
      if (!stale || stale.length === 0) break;

      for (const dog of stale) {
        const newConf = conf === "verified" ? "medium" : "low";
        await supabase
          .from("dogs")
          .update({
            intake_date: cappedDate,
            date_confidence: newConf,
            date_source: `${dog.date_source || "unknown"}|bulk_capped_${maxDays}d`,
          })
          .eq("id", dog.id);
        totalCapped++;
        confCapped++;
      }

      if (stale.length < 1000) break;
      // Don't increment offset — the update changes the rows, so next page starts fresh
      console.log(`    ...${confCapped} capped so far`);
    }
    console.log(`  ${conf}: ${confCapped} dogs capped to ${maxDays}d`);
  }

  // Step 2: Downgrade old created_date sources >1yr
  const oneYearAgo = new Date(now.getTime() - 365 * 24 * 60 * 60 * 1000);
  const { count: c1 } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .eq("is_available", true)
    .lt("intake_date", oneYearAgo.toISOString())
    .like("date_source", "%created_date%")
    .not("date_source", "like", "%available_date%")
    .in("date_confidence", ["high", "medium", "verified"]);

  if (c1 && c1 > 0) {
    await supabase
      .from("dogs")
      .update({ date_confidence: "low" })
      .eq("is_available", true)
      .lt("intake_date", oneYearAgo.toISOString())
      .like("date_source", "%created_date%")
      .not("date_source", "like", "%available_date%")
      .in("date_confidence", ["high", "medium", "verified"]);
    totalDowngraded += c1;
    console.log(`  Downgraded ${c1} created_date sources >1yr to low`);
  }

  // Step 3: Downgrade >2yr non-verified
  const twoYearsAgo = new Date(now.getTime() - 2 * 365 * 24 * 60 * 60 * 1000);
  const { count: c2 } = await supabase
    .from("dogs")
    .select("id", { count: "exact", head: true })
    .eq("is_available", true)
    .lt("intake_date", twoYearsAgo.toISOString())
    .not("date_source", "like", "%available_date%")
    .not("date_source", "like", "%found_date%")
    .neq("date_confidence", "low");

  if (c2 && c2 > 0) {
    await supabase
      .from("dogs")
      .update({ date_confidence: "low" })
      .eq("is_available", true)
      .lt("intake_date", twoYearsAgo.toISOString())
      .not("date_source", "like", "%available_date%")
      .not("date_source", "like", "%found_date%")
      .neq("date_confidence", "low");
    totalDowngraded += c2;
    console.log(`  Downgraded ${c2} non-verified sources >2yr to low`);
  }

  console.log(`\n=== DONE: ${totalCapped} capped, ${totalDowngraded} downgraded ===`);
}

run().catch(console.error);
