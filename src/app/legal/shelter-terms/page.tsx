import type { Metadata } from "next";
import Link from "next/link";

export const metadata: Metadata = {
  title: "Terms of Service for Shelter Partners | WaitingTheLongest.com",
  description:
    "Terms of Service governing the relationship between WaitingTheLongest.com and shelter/rescue partners.",
};

export default function ShelterTermsPage() {
  return (
    <div className="min-h-screen bg-white py-12 md:py-16">
      <div className="max-w-4xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="mb-8 pb-6 border-b border-gray-200">
          <h1 className="text-3xl md:text-4xl font-bold mb-2">
            Terms of Service for Shelter Partners
          </h1>
          <div className="flex items-center gap-4 text-sm text-gray-500">
            <span>Version 1.0</span>
            <span>Effective Date: March 20, 2026</span>
          </div>
        </div>

        <div className="prose prose-gray max-w-none">
          <Section title="1. Definitions">
            <p>
              <strong>&quot;Platform&quot;</strong> refers to WaitingTheLongest.com, including its website,
              APIs, and all related services operated by WaitingTheLongest (&quot;WTL&quot;, &quot;we&quot;, &quot;us&quot;, &quot;our&quot;).
            </p>
            <p>
              <strong>&quot;Shelter Partner&quot;</strong> (&quot;you&quot;, &quot;your&quot;) refers to any animal shelter, rescue
              organization, humane society, SPCA, or foster network that registers to list animals
              on the Platform.
            </p>
            <p>
              <strong>&quot;Dog Listing&quot;</strong> refers to any animal profile created on the Platform,
              including all associated data, photos, and status information.
            </p>
            <p>
              <strong>&quot;Content&quot;</strong> means all text, images, videos, data, and other materials
              submitted to or displayed on the Platform.
            </p>
            <p>
              <strong>&quot;Services&quot;</strong> means all features and functionality provided by the Platform,
              including but not limited to dog listing, search, wait time tracking, API access,
              webhook integrations, and analytics.
            </p>
          </Section>

          <Section title="2. Service Description">
            <p>
              WaitingTheLongest.com is a free platform that helps shelters and rescues increase
              visibility for their adoptable dogs. We provide:
            </p>
            <ul>
              <li>Free listing of adoptable dogs with real-time LED wait time counters</li>
              <li>Nationwide search and discovery for potential adopters</li>
              <li>API and webhook integrations for automated data sync</li>
              <li>Urgency indicators and euthanasia countdown timers for at-risk animals</li>
              <li>Analytics on listing views and engagement</li>
            </ul>
            <p>
              <strong>Core listing services are and will always remain free.</strong> We may offer
              optional premium features in the future, but participation will never be required
              for basic listing services.
            </p>
          </Section>

          <Section title="3. Shelter Partner Obligations">
            <p>As a Shelter Partner, you agree to:</p>
            <ul>
              <li>
                <strong>Accurate Data:</strong> Provide truthful, accurate, and up-to-date information
                for all dog listings, including breed, age, medical status, and availability.
              </li>
              <li>
                <strong>Timely Updates:</strong> Update listing status within 24 hours of any change
                (adoption, transfer, euthanasia, return to owner, or medical hold).
              </li>
              <li>
                <strong>No Fraudulent Listings:</strong> Not create fake, misleading, or duplicate
                listings. Not list animals from third-party breeders, pet stores, or for-profit
                entities.
              </li>
              <li>
                <strong>Legal Compliance:</strong> Comply with all applicable local, state, and federal
                animal welfare laws and regulations.
              </li>
              <li>
                <strong>Adoption Inquiry Response:</strong> Make reasonable efforts to respond to
                adoption inquiries within 48 hours.
              </li>
              <li>
                <strong>Content Standards:</strong> Not upload misleading photos, use accurate
                breed/age/medical descriptions, and ensure all listing content is truthful.
              </li>
              <li>
                <strong>API Fair Use:</strong> Use API access in accordance with rate limits and the
                API Usage Agreement. Not scrape or access other shelters&apos; data through the API.
              </li>
            </ul>
          </Section>

          <Section title="4. Platform Obligations">
            <p>WaitingTheLongest.com agrees to:</p>
            <ul>
              <li>Provide free hosting and display of shelter dog listings</li>
              <li>Maintain reasonable data security measures (see Data Sharing Agreement)</li>
              <li>Target 99.5% uptime for the Platform</li>
              <li>Remove listings within 24 hours upon shelter request</li>
              <li>Not sell shelter contact information to commercial entities</li>
              <li>Provide reasonable notice of material changes to these terms</li>
            </ul>
          </Section>

          <Section title="5. Data Ownership">
            <ul>
              <li>
                <strong>Shelter Data:</strong> You retain full ownership of all data, photos, and
                content you submit to the Platform.
              </li>
              <li>
                <strong>License Grant:</strong> By submitting content, you grant WTL a non-exclusive,
                worldwide, royalty-free license to display, index, cache, promote, and distribute
                your listings through the Platform and associated marketing channels (including
                social media).
              </li>
              <li>
                <strong>Data Portability:</strong> You may request a full export of all your data at
                any time.
              </li>
              <li>
                <strong>Data Deletion:</strong> You may request deletion of all your data at any time.
                We will comply within 30 days of written request.
              </li>
            </ul>
          </Section>

          <Section title="6. Limitation of Liability">
            <p>
              WaitingTheLongest.com is a listing platform only. We do not process adoptions,
              screen adopters, or make adoption decisions.
            </p>
            <ul>
              <li>
                WTL is not liable for adoption outcomes, including but not limited to animal
                health issues, behavioral problems, or adopter misconduct.
              </li>
              <li>
                Shelter Partners are solely responsible for screening adopters and ensuring
                appropriate placement of animals.
              </li>
              <li>
                WTL is not liable for data inaccuracies caused by shelter-provided information
                or technical issues beyond our reasonable control.
              </li>
              <li>
                IN NO EVENT SHALL WTL&apos;S TOTAL LIABILITY EXCEED THE AMOUNT PAID BY YOU TO WTL
                IN THE TWELVE (12) MONTHS PRECEDING THE CLAIM (which, for free tier users, is $0).
              </li>
            </ul>
          </Section>

          <Section title="7. Indemnification">
            <p>
              Each party agrees to indemnify, defend, and hold harmless the other party from
              any claims, damages, losses, or expenses (including reasonable attorney&apos;s fees)
              arising from:
            </p>
            <ul>
              <li>Breach of these Terms</li>
              <li>Violation of applicable law</li>
              <li>Negligent or willful misconduct</li>
              <li>Inaccurate or misleading content submitted to the Platform</li>
            </ul>
          </Section>

          <Section title="8. Termination">
            <ul>
              <li>
                Either party may terminate this agreement with 30 days written notice for any
                reason.
              </li>
              <li>
                WTL may terminate immediately for material violations of these Terms, including
                but not limited to: fraudulent listings, API abuse, or illegal activity.
              </li>
              <li>
                Upon termination, your listings will be removed within 24 hours. Data is retained
                for 90 days post-termination for administrative purposes, then permanently deleted.
              </li>
              <li>
                You may request immediate data deletion upon termination by written request.
              </li>
            </ul>
          </Section>

          <Section title="9. Dispute Resolution">
            <p>
              Any dispute arising from these Terms shall be resolved through binding arbitration
              administered by the American Arbitration Association under its Commercial Arbitration
              Rules. The arbitration shall take place in Delaware, and the laws of the State of
              Delaware shall govern these Terms.
            </p>
          </Section>

          <Section title="10. Modifications">
            <p>
              WTL may modify these Terms at any time. Material changes will be communicated
              with at least 30 days notice via email to your registered contact address and/or
              prominent notice on the Platform. Continued use of the Platform after the effective
              date of changes constitutes acceptance of the modified Terms.
            </p>
          </Section>

          <Section title="11. General Provisions">
            <ul>
              <li>
                <strong>Severability:</strong> If any provision is held invalid, the remaining
                provisions continue in full force.
              </li>
              <li>
                <strong>Entire Agreement:</strong> These Terms, together with the Data Sharing
                Agreement, Privacy Policy, and API Usage Agreement, constitute the entire
                agreement between the parties.
              </li>
              <li>
                <strong>Waiver:</strong> Failure to enforce any provision does not constitute a
                waiver of that provision.
              </li>
              <li>
                <strong>Assignment:</strong> You may not assign these Terms without our prior
                written consent. WTL may assign these Terms in connection with a merger,
                acquisition, or sale of all or substantially all of its assets.
              </li>
            </ul>
          </Section>

          <Section title="12. Contact">
            <p>
              For questions about these Terms, contact us at:{" "}
              <a href="mailto:legal@waitingthelongest.com" className="text-green-700 hover:underline">
                legal@waitingthelongest.com
              </a>
            </p>
          </Section>
        </div>

        <div className="mt-12 pt-6 border-t border-gray-200 text-sm text-gray-500">
          <p>
            See also:{" "}
            <Link href="/legal/data-sharing" className="text-green-700 hover:underline">
              Data Sharing Agreement
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
