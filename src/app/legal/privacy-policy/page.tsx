import type { Metadata } from "next";
import Link from "next/link";

export const metadata: Metadata = {
  title: "Privacy Policy | WaitingTheLongest.com",
  description:
    "Privacy Policy for WaitingTheLongest.com — how we collect, use, and protect your information.",
};

export default function PrivacyPolicyPage() {
  return (
    <div className="min-h-screen bg-white py-12 md:py-16">
      <div className="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="mb-8 pb-6 border-b border-gray-200">
          <h1 className="text-3xl md:text-4xl font-bold mb-2">
            Privacy Policy
          </h1>
          <div className="flex items-center gap-4 text-sm text-gray-500">
            <span>Version 1.0</span>
            <span>Effective Date: March 20, 2026</span>
          </div>
        </div>

        <div className="prose prose-gray max-w-none">
          <Section title="1. Information We Collect">
            <h3 className="text-lg font-semibold mt-4 mb-2">From Shelter Partners</h3>
            <ul>
              <li>Organization information (name, address, phone, email, website, EIN)</li>
              <li>Contact person details (name, title, email, phone)</li>
              <li>Dog listing data (profiles, photos, status, medical info)</li>
              <li>Integration data (API usage, webhook configurations)</li>
              <li>Communication history with WTL</li>
            </ul>

            <h3 className="text-lg font-semibold mt-4 mb-2">Dog Listing Data Sources</h3>
            <p>Dog listings displayed on the Platform are aggregated from the following sources:</p>
            <ul>
              <li>RescueGroups.org API v5 (primary database of ~33,000+ dog listings)</li>
              <li>Direct shelter website scraping via 8 automated platform adapters (ShelterLuv, ShelterBuddy, Petango, Petstablished, Chameleon/PetHarbor, ShelterManager/ASM, RescueGroups Widget, and generic HTML parsing)</li>
              <li>Shelter-submitted CSV imports and API feeds via our partner portal</li>
              <li>Automated verification and freshness monitoring systems</li>
            </ul>
            <p>Each dog listing includes a Data Sources page documenting the specific provenance of that listing&apos;s data.</p>

            <h3 className="text-lg font-semibold mt-4 mb-2">From Adopters / Website Visitors</h3>
            <ul>
              <li>Information voluntarily provided (name, email when submitting interest forms or foster applications)</li>
              <li>Search and browsing behavior (pages viewed, dogs viewed, filters used)</li>
              <li>Saved/favorited dogs (stored locally in browser, not on our servers)</li>
              <li>Device and browser information for analytics purposes</li>
            </ul>

            <h3 className="text-lg font-semibold mt-4 mb-2">Automatically Collected</h3>
            <ul>
              <li>IP addresses (hashed for privacy when used in analytics)</li>
              <li>Browser type and version</li>
              <li>Pages visited and time spent</li>
              <li>Referring website</li>
            </ul>
          </Section>

          <Section title="2. How We Use Information">
            <ul>
              <li><strong>Display Listings:</strong> Showing adoptable dogs to potential adopters</li>
              <li><strong>Facilitate Adoptions:</strong> Connecting interested adopters with shelters (via redirect to shelter website or contact info)</li>
              <li><strong>Send Notifications:</strong> Communicating with shelter partners about their accounts, listings, and platform updates</li>
              <li><strong>Improve Service:</strong> Analyzing usage patterns to improve platform features, search relevance, and user experience</li>
              <li><strong>Generate Analytics:</strong> Creating aggregate, anonymized statistics about shelter dog populations and adoption trends</li>
              <li><strong>Prevent Abuse:</strong> Rate limiting, fraud detection, and ensuring platform security</li>
            </ul>
          </Section>

          <Section title="3. Information Sharing">
            <p><strong>We share information in the following ways:</strong></p>
            <ul>
              <li><strong>Public Dog Listings:</strong> Dog profiles, photos, and shelter contact information are displayed publicly on the Platform to facilitate adoptions.</li>
              <li><strong>Aggregate Statistics:</strong> Anonymized, aggregate data about dog populations and adoption trends may be shared publicly or with research organizations.</li>
              <li><strong>Legal Requirements:</strong> We may disclose information when required by law, regulation, legal process, or government request.</li>
              <li><strong>Service Providers:</strong> We share data with service providers (sub-processors) who help us operate the Platform, subject to confidentiality agreements.</li>
            </ul>
            <p className="font-semibold mt-4">We do NOT:</p>
            <ul>
              <li>Sell personal information to anyone, ever.</li>
              <li>Share shelter staff personal contact details with marketers or advertisers.</li>
              <li>Use personal information for targeted advertising.</li>
              <li>Share data with data brokers.</li>
            </ul>
          </Section>

          <Section title="4. Cookies & Tracking">
            <ul>
              <li><strong>Essential Cookies:</strong> Used for authentication (login sessions) and basic platform functionality. These cannot be disabled.</li>
              <li><strong>Local Storage:</strong> Used to store saved/favorited dogs on your device. This data never leaves your browser.</li>
              <li><strong>Analytics:</strong> We may use privacy-respecting analytics to understand how the Platform is used. We do not use Google Analytics or other invasive tracking.</li>
              <li><strong>No Third-Party Ad Trackers:</strong> We do not use advertising trackers, retargeting pixels, or any third-party advertising technology.</li>
            </ul>
          </Section>

          <Section title="5. Data Security">
            <p>We protect your information using:</p>
            <ul>
              <li>TLS encryption for all data in transit</li>
              <li>AES-256 encryption for all data at rest</li>
              <li>Row-Level Security (RLS) in our database</li>
              <li>SHA-256 hashing for API keys (raw keys never stored)</li>
              <li>HMAC-SHA256 signing for webhook deliveries</li>
              <li>Regular security audits and access controls</li>
              <li>IP address hashing for analytics (never stored in raw form)</li>
            </ul>
          </Section>

          <Section title="6. Your Rights">
            <p>Regardless of where you are located, you have the following rights:</p>
            <ul>
              <li><strong>Access:</strong> Request a copy of all personal information we hold about you.</li>
              <li><strong>Correction:</strong> Request correction of inaccurate personal information.</li>
              <li><strong>Deletion:</strong> Request permanent deletion of your personal information.</li>
              <li><strong>Portability:</strong> Receive your data in a machine-readable format (JSON, CSV).</li>
              <li><strong>Opt-Out of Marketing:</strong> Unsubscribe from marketing communications at any time.</li>
            </ul>
            <p>
              To exercise any of these rights, email{" "}
              <a href="mailto:privacy@waitingthelongest.com" className="text-green-700 hover:underline">
                privacy@waitingthelongest.com
              </a>
              . We will respond within 30 days.
            </p>
          </Section>

          <Section title="7. Children's Privacy">
            <p>
              WaitingTheLongest.com is not directed at children under the age of 13. We do not
              knowingly collect personal information from children under 13. If we discover we
              have collected such information, we will delete it promptly. If you believe a child
              under 13 has provided us with personal information, please contact us at{" "}
              <a href="mailto:privacy@waitingthelongest.com" className="text-green-700 hover:underline">
                privacy@waitingthelongest.com
              </a>
              .
            </p>
          </Section>

          <Section title="8. State-Specific Rights">
            <h3 className="text-lg font-semibold mt-4 mb-2">California (CCPA/CPRA)</h3>
            <p>
              California residents have the right to know what personal information is collected,
              request deletion, opt out of sale (we do not sell data), and not be discriminated
              against for exercising these rights. To make a request, email
              privacy@waitingthelongest.com with subject line &quot;CCPA Request&quot;.
            </p>

            <h3 className="text-lg font-semibold mt-4 mb-2">Virginia (VCDPA)</h3>
            <p>
              Virginia consumers have rights to access, correct, delete, obtain a copy of, and
              opt out of processing of personal data for targeted advertising or sale.
            </p>

            <h3 className="text-lg font-semibold mt-4 mb-2">Colorado (CPA) & Connecticut (CTDPA)</h3>
            <p>
              Residents of Colorado and Connecticut have similar rights to access, correct, delete,
              and port their personal data, and to opt out of targeted advertising, sale, or
              profiling.
            </p>

            <p className="mt-4">
              All state-specific requests will be processed within the timeframes required by
              applicable law (typically 45 days, with one 45-day extension if needed).
            </p>
          </Section>

          <Section title="9. Changes to This Policy">
            <p>
              We may update this Privacy Policy from time to time. Material changes will be
              communicated via email to registered users and/or prominent notice on the Platform
              at least 30 days before taking effect. Continued use after the effective date
              constitutes acceptance.
            </p>
          </Section>

          <Section title="10. Contact Information">
            <p>For privacy-related questions, requests, or concerns:</p>
            <ul>
              <li>
                Email:{" "}
                <a href="mailto:privacy@waitingthelongest.com" className="text-green-700 hover:underline">
                  privacy@waitingthelongest.com
                </a>
              </li>
              <li>Website: <a href="https://waitingthelongest.com" className="text-green-700 hover:underline">waitingthelongest.com</a></li>
            </ul>
          </Section>
        </div>

        <div className="mt-12 pt-6 border-t border-gray-200 text-sm text-gray-500">
          <p>
            See also:{" "}
            <Link href="/legal/shelter-terms" className="text-green-700 hover:underline">
              Terms of Service
            </Link>{" "}
            |{" "}
            <Link href="/legal/data-sharing" className="text-green-700 hover:underline">
              Data Sharing Agreement
            </Link>{" "}
            |{" "}
            <Link href="/legal/api-terms" className="text-green-700 hover:underline">
              API Usage Agreement
            </Link>
          </p>
        </div>
      </div>
    </div>
  );
}

function Section({
  title,
  children,
}: {
  title: string;
  children: React.ReactNode;
}) {
  return (
    <section className="mb-8">
      <h2 className="text-xl font-bold mb-3">{title}</h2>
      {children}
    </section>
  );
}
