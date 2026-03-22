import type { Metadata } from "next";
import RegistrationForm from "@/components/partners/RegistrationForm";

export const metadata: Metadata = {
  title: "Register Your Organization | WaitingTheLongest.com",
  description:
    "Register your shelter or rescue to list dogs for free on WaitingTheLongest.com. 5-minute setup, real-time sync, nationwide reach.",
  openGraph: {
    title: "Register Your Shelter | WaitingTheLongest.com",
    description:
      "Join thousands of shelters listing adoptable dogs for free. Register in 5 minutes.",
  },
};

export default function RegisterPage() {
  return (
    <div className="min-h-screen bg-gray-50 py-12 md:py-16">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="text-center mb-10">
          <h1 className="text-3xl md:text-4xl font-bold mb-3">
            Register Your Organization
          </h1>
          <p className="text-lg text-gray-600 max-w-2xl mx-auto">
            Complete the form below to list your dogs on
            WaitingTheLongest.com. It&apos;s free, takes about 5 minutes, and
            your dogs could be live today.
          </p>
        </div>
        <RegistrationForm />
      </div>
    </div>
  );
}
