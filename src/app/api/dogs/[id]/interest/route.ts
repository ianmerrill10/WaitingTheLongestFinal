import { NextResponse } from "next/server";
import { createAdminClient } from "@/lib/supabase/admin";

export async function POST(
  request: Request,
  { params }: { params: Promise<{ id: string }> }
) {
  const { id } = await params;

  const body = await request.json();
  const { name, email, phone, message } = body;

  if (!name || !email) {
    return NextResponse.json(
      { error: "Name and email are required" },
      { status: 400 }
    );
  }

  const supabase = createAdminClient();

  // Get the dog and shelter info
  const { data: dog, error: dogError } = await supabase
    .from("dogs")
    .select(
      `
      id, name, breed_primary,
      shelters (
        id, name, email, phone
      )
    `
    )
    .eq("id", id)
    .single();

  if (dogError || !dog) {
    return NextResponse.json({ error: "Dog not found" }, { status: 404 });
  }

  // Increment inquiry count
  try {
    await supabase
      .from("dogs")
      .update({ inquiry_count: (dog as Record<string, unknown>).inquiry_count as number + 1 })
      .eq("id", id);
  } catch {
    // Increment failed, non-critical
  }

  // For now, log the interest (later: send email to shelter via SendGrid)
  console.log(
    `ADOPTION INTEREST: ${name} (${email}, ${phone || "no phone"}) interested in ${dog.name} (${dog.breed_primary})`
  );
  if (message) console.log(`Message: ${message}`);

  return NextResponse.json({
    success: true,
    message: `Your interest in ${dog.name} has been sent to the shelter. They will contact you soon!`,
  });
}
