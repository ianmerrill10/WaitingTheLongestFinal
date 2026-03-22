import type { Metadata } from "next";
import Link from "next/link";

export const metadata: Metadata = {
  title: "Data Sharing Agreement | WaitingTheLongest.com",
  description:
    "Data Sharing Agreement governing how WaitingTheLongest.com collects, uses, and protects shelter partner data.",
};

export default function DataSharingPage() {
  return (
    <div className="min-h-screen bg-white py-12 md:py-16">
      <div className="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="mb-8 pb-6 border-b border-gray-200">
          <h1 className="text-3xl md:text-4xl font-bold mb-2">
            Data Sharing Agreement
          </h1>
          <div className="flex items-center gap-4 text-sm text-gray-500">
            <span>Version 1.0</span>
            <span>Effective Date: March 20, 2026</span>
          </div>
        </div>

        <div className="prose prose-gray max-w-none">
          <Section title="1. Scope of Data Shared">
            <p>This agreement covers the following categories of data shared between Shelter Partners and WaitingTheLongest.com (&quot;WTL&quot;):</p>
            <ul>
              <li><strong>Dog Profiles:</strong> Name, breed, age, size, weight, sex, color, medical status, temperament, special needs, adoption fee, photos, videos, and descriptions.</li>
              <li><strong>Shelter Information:</strong> Organization name, address, phone, email, website, hours of operation, and type classification.</li>
              <li><strong>Contact Information:</strong> Names, titles, email addresses, and phone numbers of shelter staff designated as points of contact.</li>
              <li><strong>Status Data:</strong> Availability status, intake dates, euthanasia dates, adoption dates, and urgency classifications.</li>
              <li><strong>Integration Data:</strong> API keys (hashed), webhook endpoints, and usage logs.</li>
            </ul>
          </Section>

          <Section title="2. Permitted Uses">
            <p>WTL may use shared data for the following purposes:</p>
            <ul>
              <li><strong>Public Display:</strong> Displaying dog profiles and shelter information on the WTL website to facilitate adoptions.</li>
              <li><strong>Search Indexing:</strong> Making listings discoverable through on-site search and external search engines.</li>
              <li><strong>Social Media Promotion:</strong> Featuring dogs and shelters on WTL&apos;s social media accounts to increase adoption visibility.</li>
              <li><strong>Aggregate Analytics:</strong> Generating anonymized, aggregate statistics about shelter dog populations, wait times, and adoption trends.</li>
              <li><strong>API Distribution:</strong> Making listings available through WTL&apos;s API to approved partners and integrators.</li>
              <li><strong>Platform Improvement:</strong> Analyzing usage patterns to improve the Platform&apos;s features and user experience.</li>
            </ul>
          </Section>

          <Section title="3. Prohibited Uses">
            <p>WTL will NOT:</p>
            <ul>
              <li>Sell raw shelter data or contact information to third parties.</li>
              <li>Share shelter staff personal contact information with commercial marketers, advertisers, or data brokers.</li>
              <li>Use shelter data to build competing products or services without consent.</li>
              <li>Share individual shelter performance data with other shelters without consent.</li>
              <li>Use shelter logos or branding in paid advertising without express permission.</li>
            </ul>
          </Section>

          <Section title="4. Data Retention">
            <ul>
              <li><strong>Active Listings:</strong> Retained and displayed for as long as the dog is marked available or the shelter partnership is active.</li>
              <li><strong>Removed/Adopted Listings:</strong> Archived for 1 year for historical analytics and &quot;Happy Tails&quot; features, then permanently deleted.</li>
              <li><strong>Shelter Profile Data:</strong> Retained for the duration of the partnership plus 90 days post-termination.</li>
              <li><strong>Communication Logs:</strong> Retained for 2 years for dispute resolution and service improvement purposes.</li>
              <li><strong>API Usage Logs:</strong> Retained for 90 days for security and debugging, then aggregated and anonymized.</li>
              <li><strong>Full Deletion:</strong> Upon written request, all shelter data will be permanently deleted within 30 days. This action is irreversible.</li>
            </ul>
          </Section>

          <Section title="5. Security Measures">
            <p>WTL implements the following security measures to protect shared data:</p>
            <ul>
              <li><strong>Encryption in Transit:</strong> All data transmitted between shelters and the Platform uses TLS 1.2 or higher encryption.</li>
              <li><strong>Encryption at Rest:</strong> All data stored in our database (Supabase/PostgreSQL) is encrypted at rest using AES-256.</li>
              <li><strong>Access Controls:</strong> Database access is restricted to authorized systems and personnel. Row-Level Security (RLS) policies enforce data isolation.</li>
              <li><strong>API Key Security:</strong> API keys are stored as SHA-256 hashes. Raw keys are never stored and cannot be retrieved after initial generation.</li>
              <li><strong>Webhook Signing:</strong> All webhook deliveries are signed with HMAC-SHA256 to ensure integrity and authenticity.</li>
              <li><strong>Regular Audits:</strong> We conduct regular security reviews and maintain audit logs of data access.</li>
            </ul>
          </Section>

          <Section title="6. Breach Notification">
            <p>
              In the event of a data breach that affects shelter partner data, WTL will:
            </p>
            <ul>
              <li>Notify affected Shelter Partners within <strong>72 hours</strong> of discovering the breach.</li>
              <li>Provide details of the breach, including what data was affected and what remediation steps are being taken.</li>
              <li>Cooperate fully with any investigations and implement measures to prevent future breaches.</li>
              <li>Notify relevant regulatory authorities as required by applicable law.</li>
            </ul>
          </Section>

          <Section title="7. Data Subject Rights">
            <p>
              WTL supports data subject rights in compliance with applicable privacy laws:
            </p>
            <ul>
              <li><strong>Right to Access:</strong> Shelter Partners may request a complete export of all their data at any time.</li>
              <li><strong>Right to Correction:</strong> Inaccurate data will be corrected upon notification.</li>
              <li><strong>Right to Deletion:</strong> Data will be permanently deleted within 30 days of written request.</li>
              <li><strong>Right to Portability:</strong> Data exports are provided in machine-readable formats (JSON, CSV).</li>
              <li><strong>Annual Report:</strong> An annual data processing report is available upon request, detailing what data is held and how it is used.</li>
            </ul>
          </Section>

          <Section title="8. Cross-Border Transfers">
            <p>
              All data is processed and stored in the United States (specifically, the Supabase East US — North Virginia region). No personal data is transferred internationally without prior written consent from the Shelter Partner.
            </p>
          </Section>

          <Section title="9. Sub-Processors">
            <p>WTL uses the following sub-processors to provide its services:</p>
            <ul>
              <li><strong>Supabase</strong> — Database hosting and authentication (<a href="https://supabase.com/privacy" className="text-green-700 hover:underline" target="_blank" rel="noopener noreferrer">Privacy Policy</a>)</li>
              <li><strong>Vercel</strong> — Website hosting and serverless functions (<a href="https://vercel.com/legal/privacy-policy" className="text-green-700 hover:underline" target="_blank" rel="noopener noreferrer">Privacy Policy</a>)</li>
              <li><strong>SendGrid (Twilio)</strong> — Email delivery (<a href="https://www.twilio.com/en-us/legal/privacy" className="text-green-700 hover:underline" target="_blank" rel="noopener noreferrer">Privacy Policy</a>)</li>
            </ul>
            <p>
              WTL will notify Shelter Partners of any material changes to sub-processors with at least 30 days notice.
            </p>
          </Section>

          <Section title="10. Contact">
            <p>
              For questions about this Data Sharing Agreement, data requests, or breach notifications:{" "}
              <a href="mailto:privacy@waitingthelongest.com" className="text-green-700 hover:underline">
                privacy@waitingthelongest.com
              </a>
            </p>
          </Section>
        </div>

        <div className="mt-12 pt-6 border-t border-gray-200 text-sm text-gray-500">
          <p>
            See also:{" "}
            <Link href="/legal/shelter-terms" className="text-green-700 hover:underline">
              Terms of Service
            </Link>{" "}
            |{" "}
            <Link href="/legal/privacy-policy" className="text-green-700 hover:underline">
              Privacy Policy
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
