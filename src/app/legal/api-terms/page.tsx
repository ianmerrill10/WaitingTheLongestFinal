import type { Metadata } from "next";
import Link from "next/link";

export const metadata: Metadata = {
  title: "API Usage Agreement | WaitingTheLongest.com",
  description:
    "API Usage Agreement governing access to and use of the WaitingTheLongest.com REST API.",
};

export default function ApiTermsPage() {
  return (
    <div className="min-h-screen bg-white py-12 md:py-16">
      <div className="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="mb-8 pb-6 border-b border-gray-200">
          <h1 className="text-3xl md:text-4xl font-bold mb-2">
            API Usage Agreement
          </h1>
          <div className="flex items-center gap-4 text-sm text-gray-500">
            <span>Version 1.0</span>
            <span>Effective Date: March 20, 2026</span>
          </div>
        </div>

        <div className="prose prose-gray max-w-none">
          <Section title="1. API Access">
            <p>
              Access to the WaitingTheLongest.com API (&quot;API&quot;) is granted upon registration
              as a Shelter Partner and issuance of an API key. API access is:
            </p>
            <ul>
              <li>Non-exclusive and non-transferable</li>
              <li>Subject to these terms and the general Terms of Service</li>
              <li>Revocable at any time for violation of these terms</li>
              <li>Provided free of charge for the Free tier</li>
            </ul>
          </Section>

          <Section title="2. Acceptable Use">
            <p>The API may be used for:</p>
            <ul>
              <li>Managing your own shelter&apos;s dog listings (create, read, update, delete)</li>
              <li>Integrating WTL data with your own shelter management software</li>
              <li>Building internal tools and dashboards for your organization</li>
              <li>Receiving webhook notifications about your dogs</li>
              <li>Generating reports and analytics about your shelter&apos;s performance</li>
              <li>Automated data sync between your systems and WTL</li>
            </ul>
          </Section>

          <Section title="3. Prohibited Use">
            <p>The following uses are strictly prohibited:</p>
            <ul>
              <li><strong>Scraping:</strong> Accessing or scraping other shelters&apos; data, whether through the API or any other means.</li>
              <li><strong>Competitive Use:</strong> Using API data to build competing platforms or services without explicit written permission.</li>
              <li><strong>Rate Limit Circumvention:</strong> Attempting to bypass rate limits through multiple keys, distributed requests, or other techniques.</li>
              <li><strong>Key Sharing:</strong> Sharing API keys with unauthorized third parties. Each integration must use its own key.</li>
              <li><strong>Fake Listings:</strong> Using the API to create fraudulent, misleading, or spam dog listings.</li>
              <li><strong>Automated Attacks:</strong> Using the API for denial-of-service attacks, vulnerability scanning, or other security testing without prior written authorization.</li>
              <li><strong>Data Resale:</strong> Selling or redistributing data obtained through the API.</li>
            </ul>
          </Section>

          <Section title="4. Rate Limits">
            <p>API usage is subject to rate limits based on your partner tier:</p>
            <div className="overflow-x-auto">
              <table className="min-w-full border border-gray-300 mt-4">
                <thead className="bg-gray-50">
                  <tr>
                    <th className="px-4 py-2 text-left border-b">Tier</th>
                    <th className="px-4 py-2 text-left border-b">Per Minute</th>
                    <th className="px-4 py-2 text-left border-b">Per Hour</th>
                    <th className="px-4 py-2 text-left border-b">Per Day</th>
                  </tr>
                </thead>
                <tbody>
                  <tr>
                    <td className="px-4 py-2 border-b">Free</td>
                    <td className="px-4 py-2 border-b">60 requests</td>
                    <td className="px-4 py-2 border-b">1,000 requests</td>
                    <td className="px-4 py-2 border-b">10,000 requests</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-2 border-b">Basic</td>
                    <td className="px-4 py-2 border-b">120 requests</td>
                    <td className="px-4 py-2 border-b">5,000 requests</td>
                    <td className="px-4 py-2 border-b">50,000 requests</td>
                  </tr>
                  <tr>
                    <td className="px-4 py-2 border-b">Premium</td>
                    <td className="px-4 py-2 border-b">300 requests</td>
                    <td className="px-4 py-2 border-b">20,000 requests</td>
                    <td className="px-4 py-2 border-b">200,000 requests</td>
                  </tr>
                </tbody>
              </table>
            </div>
            <p className="mt-4">
              When rate limits are exceeded, the API returns HTTP 429 (Too Many Requests) with
              a <code>Retry-After</code> header indicating when the next request may be made.
              Rate limit headers (<code>X-RateLimit-Limit</code>, <code>X-RateLimit-Remaining</code>,
              <code>X-RateLimit-Reset</code>) are included in every response.
            </p>
          </Section>

          <Section title="5. Availability">
            <ul>
              <li>The API is provided on a <strong>best-effort basis</strong> for the Free tier.</li>
              <li>No guaranteed SLA for Free tier users. Basic and Premium tiers may include SLA commitments as specified in their service agreement.</li>
              <li>Scheduled maintenance windows will be announced at least 48 hours in advance via email to registered API key holders.</li>
              <li>Emergency maintenance may occur without notice but will be communicated as soon as possible.</li>
            </ul>
          </Section>

          <Section title="6. API Versioning">
            <ul>
              <li>The current API version is <code>v1</code>, accessible at <code>/api/v1/*</code>.</li>
              <li>Deprecated API versions will be supported for a minimum of <strong>12 months</strong> after deprecation notice.</li>
              <li>Migration guides will be provided when new versions are released.</li>
              <li>Breaking changes will only be introduced in new major versions.</li>
              <li>Non-breaking additions (new fields, new endpoints) may be added to the current version at any time.</li>
            </ul>
          </Section>

          <Section title="7. Data Format">
            <ul>
              <li>All API responses use <strong>JSON</strong> format with UTF-8 encoding.</li>
              <li>Dates follow <strong>ISO 8601</strong> format (e.g., <code>2026-03-20T12:00:00Z</code>).</li>
              <li>IDs use <strong>UUID v4</strong> format.</li>
              <li>Pagination follows cursor-based or offset-based patterns as documented per endpoint.</li>
              <li>Error responses include a consistent structure: <code>{`{ "error": "message", "code": "ERROR_CODE" }`}</code></li>
            </ul>
          </Section>

          <Section title="8. Authentication">
            <ul>
              <li>All API requests require authentication via an API key passed in the <code>Authorization: Bearer wtl_sk_...</code> header.</li>
              <li><strong>HTTPS is mandatory.</strong> HTTP requests will be rejected.</li>
              <li>API keys must be stored securely and never exposed in client-side code, public repositories, or browser-accessible locations.</li>
              <li>If a key is compromised, rotate it immediately via the dashboard or API.</li>
              <li>Keys can be scoped with specific permissions (read, write, delete, webhook_manage, bulk_import).</li>
            </ul>
          </Section>

          <Section title="9. Liability">
            <ul>
              <li>WTL is not liable for data loss resulting from API misuse, including but not limited to: unintended bulk deletions, malformed update requests, or integration bugs.</li>
              <li>Shelter Partners are responsible for maintaining their own backups of critical data.</li>
              <li>WTL provides no warranty regarding API availability, accuracy, or fitness for any particular purpose beyond what is specified in the applicable service tier agreement.</li>
            </ul>
          </Section>

          <Section title="10. Termination">
            <ul>
              <li>API access may be revoked immediately for violations of this agreement or the Terms of Service.</li>
              <li>For non-violation revocation (e.g., account closure), 30 days notice will be provided.</li>
              <li>Upon termination, all API keys will be deactivated and outstanding webhook deliveries will cease.</li>
              <li>API request logs will be retained for 90 days post-termination for security audit purposes.</li>
            </ul>
          </Section>

          <Section title="11. Contact">
            <p>
              For API support, technical questions, or to report issues:{" "}
              <a href="mailto:api@waitingthelongest.com" className="text-green-700 hover:underline">
                api@waitingthelongest.com
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
            <Link href="/legal/data-sharing" className="text-green-700 hover:underline">
              Data Sharing Agreement
            </Link>{" "}
            |{" "}
            <Link href="/legal/privacy-policy" className="text-green-700 hover:underline">
              Privacy Policy
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
