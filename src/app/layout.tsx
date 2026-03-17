import type { Metadata } from "next";
import "./globals.css";
import Header from "@/components/layout/Header";
import Footer from "@/components/layout/Footer";

export const metadata: Metadata = {
  title: {
    default: "WaitingTheLongest.com - Save Shelter Dogs",
    template: "%s | WaitingTheLongest.com",
  },
  description:
    "Find adoptable dogs from shelters and rescues across the United States. Real-time euthanasia countdown timers help you save dogs before time runs out. Because every day matters.",
  keywords: [
    "adopt a dog",
    "shelter dogs",
    "rescue dogs",
    "euthanasia list",
    "urgent dogs",
    "dog adoption",
    "animal rescue",
    "save a dog",
  ],
  openGraph: {
    title: "WaitingTheLongest.com - Because Every Day Matters",
    description:
      "Find and save adoptable shelter dogs across America. Real-time countdown timers for dogs on euthanasia lists.",
    url: "https://waitingthelongest.com",
    siteName: "WaitingTheLongest.com",
    type: "website",
  },
  twitter: {
    card: "summary_large_image",
    title: "WaitingTheLongest.com - Save Shelter Dogs",
    description:
      "Real-time euthanasia countdowns for shelter dogs. Help save a life today.",
  },
  robots: {
    index: true,
    follow: true,
  },
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en">
      <body className="min-h-screen bg-gray-50 text-gray-900 flex flex-col">
        <Header />
        <main className="flex-1">{children}</main>
        <Footer />
      </body>
    </html>
  );
}
