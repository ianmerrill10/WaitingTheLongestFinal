import type { Metadata, Viewport } from "next";
import "./globals.css";
import Header from "@/components/layout/Header";
import Footer from "@/components/layout/Footer";
import BottomTabs from "@/components/layout/BottomTabs";
import ErrorBoundary from "@/components/ui/ErrorBoundary";
import UrgentTicker from "@/components/ui/UrgentTicker";
import { ToastProvider } from "@/components/ui/Toast";
import { generateOrganizationJsonLd } from "@/lib/utils/json-ld";

export const metadata: Metadata = {
  title: {
    default: "WaitingTheLongest.com - Save Shelter Dogs",
    template: "%s | WaitingTheLongest.com",
  },
  description:
    "Find adoptable dogs from shelters and rescues across the United States. Real-time euthanasia countdown timers help you save dogs before time runs out. Stop a clock, save a life.",
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
    title: "WaitingTheLongest.com - Stop a Clock, Save a Life",
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
  manifest: "/manifest.json",
};

export const viewport: Viewport = {
  themeColor: "#00ff41",
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en">
      <body className="min-h-screen bg-gray-50 text-gray-900 flex flex-col">
        <a href="#main-content" className="sr-only focus:not-sr-only focus:fixed focus:top-2 focus:left-2 focus:z-[200] focus:px-4 focus:py-2 focus:bg-blue-600 focus:text-white focus:rounded-lg focus:font-medium">
          Skip to content
        </a>
        <script
          type="application/ld+json"
          dangerouslySetInnerHTML={{
            __html: JSON.stringify(generateOrganizationJsonLd()),
          }}
        />
        <ToastProvider>
          <Header />
          <UrgentTicker />
          <main id="main-content" className="flex-1 pb-16 md:pb-0">
            <ErrorBoundary>{children}</ErrorBoundary>
          </main>
          <Footer />
          <BottomTabs />
        </ToastProvider>
      </body>
    </html>
  );
}
