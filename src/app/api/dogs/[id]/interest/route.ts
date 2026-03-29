import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";

const EMAIL_REGEX = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;

export async function POST(
  request: Request,
  { params }: { params: Promise<{ id: string }> }
) {
  const { id } = await params;

  let body;
  try {
    body = await request.json();
  } catch {
    return NextResponse.json(
      { error: "Invalid request body" },
      { status: 400 }
    );
  }

  const { name, email, phone, message } = body;

  if (!name || typeof name !== "string" || name.trim().length < 2) {
    return NextResponse.json(
      { error: "Please provide your full name (at least 2 characters)" },
      { status: 400 }
    );
  }

  if (!email || typeof email !== "string" || !EMAIL_REGEX.test(email)) {
    return NextResponse.json(
      { error: "Please provide a valid email address" },
      { status: 400 }
    );
  }

  const supabase = createAdminClient();

  // Get the dog and shelter info
  const { data: dog, error: dogError } = await supabase
    .from("dogs")
    .select(
      `
      id, name, breed_primary, inquiry_count, external_url,
      shelters (
        id, name, email, phone, website
      )
    `
    )
    .eq("id", id)
    .single();

  if (dogError || !dog) {
    return NextResponse.json({ error: "Dog not found" }, { status: 404 });
  }

  // Increment inquiry count safely
  const currentCount =
    typeof dog.inquiry_count === "number" ? dog.inquiry_count : 0;
  await supabase
    .from("dogs")
    .update({ inquiry_count: currentCount + 1 })
    .eq("id", id);

  // Build shelter contact info
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const shelterData = dog.shelters as any;
  const shelter = Array.isArray(shelterData) ? shelterData[0] : shelterData;
  const shelterName = shelter?.name || "the shelter";
  const shelterUrl = dog.external_url || shelter?.website || null;

  // Log adoption interest
  console.log(
    `ADOPTION INTEREST: ${name.trim()} (${email.trim()}, ${phone || "no phone"}) interested in ${dog.name} (${dog.breed_primary}) [dog_id: ${id}]`
  );

  // Record as a communication for the shelter's dashboard
  if (shelter?.id) {
    const { error: commError } = await supabase.from("shelter_communications").insert({
      shelter_id: shelter.id,
      comm_type: "adoption_inquiry",
      direction: "inbound",
      subject: `Adoption inquiry for ${dog.name} from ${name.trim()}`,
      body: `Name: ${name.trim()}\nEmail: ${email.trim()}\nPhone: ${phone || "N/A"}\n\n${message || "No message provided."}`,
      to_address: shelter?.email || null,
      status: "received",
    });
    if (commError) {
      console.error(`Failed to log adoption inquiry for dog ${id}:`, commError.message);
    }
  }

  return NextResponse.json({
    success: true,
    message: `Thank you for your interest in ${dog.name}! Please contact ${shelterName} directly to arrange a meeting.`,
    shelter_name: shelterName,
    shelter_phone: shelter?.phone || null,
    shelter_email: shelter?.email || null,
    shelter_url: shelterUrl,
    dog_url: dog.external_url || null,
  });
}
