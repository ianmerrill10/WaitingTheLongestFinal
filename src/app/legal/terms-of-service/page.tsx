import type { Metadata } from "next";
import Link from "next/link";

export const metadata: Metadata = {
  title: "Terms of Service | WaitingTheLongest.com",
  description: "Terms of Service governing the use of WaitingTheLongest.com.",
};

export default function TermsOfServicePage() {
  return (
    <div className="min-h-screen bg-white">
      <div className="max-w-3xl mx-auto px-4 py-12">
        <h1 className="text-3xl font-bold text-gray-900 mb-2">
          Terms of Service
        </h1>
        <p className="text-sm text-gray-500 mb-8">
          Last updated: March 25, 2026
        </p>

        <div className="prose prose-gray max-w-none space-y-6 text-gray-700 text-sm leading-relaxed">
          <section>
            <h2 className="text-lg font-semibold text-gray-900 mt-8 mb-3">
              1. Acceptance of Terms
            </h2>
            <p>
              By accessing or using WaitingTheLongest.com (&quot;the Platform&quot;), you agree
              to be bound by these Terms of Service (&quot;Terms&quot;). If you do not agree
              to these Terms, you must not use the Platform.
            </p>
          </section>

          <section>
            <h2 className="text-lg font-semibold text-gray-900 mt-8 mb-3">
              2. Description of Service
            </h2>
            <p>
              WaitingTheLongest.com is a free informational platform that aggregates
              publicly available data about adoptable dogs in animal shelters, rescues,
              and nonprofit agencies across the United States. The Platform displays
              estimated wait times and, where available, estimated euthanasia countdown
              timers to encourage timely adoption and rescue.
            </p>
            <p className="mt-2">
              <strong>The Platform is NOT an adoption agency.</strong> We do not
              facilitate, process, or guarantee adoptions. All adoption inquiries are
              redirected to the respective shelter or rescue organization. The Platform
              does not take custody of any animal.
            </p>
          </section>

          <section>
            <h2 className="text-lg font-semibold text-gray-900 mt-8 mb-3">
              3. Data Sources and Accuracy
            </h2>
            <p>
              Dog listing data is aggregated from multiple third-party sources, including
              but not limited to:
            </p>
            <ul className="list-disc ml-6 mt-2 space-y-1">
              <li>RescueGroups.org API (primary database)</li>
              <li>Direct shelter website scraping via automated platform adapters (ShelterLuv, ShelterBuddy, Petango, Petstablished, Chameleon/PetHarbor, ShelterManager/ASM, and others)</li>
              <li>Shelter-submitted CSV/API imports via our partner portal</li>
              <li>Automated verification and monitoring systems</li>
            </ul>
            <p className="mt-3 font-semibold">
              IMPORTANT DISCLAIMER REGARDING DATA ACCURACY:
            </p>
            <ul className="list-disc ml-6 mt-2 space-y-2">
              <li>
                All data displayed on the Platform is provided for <strong>informational purposes only</strong>.
              </li>
              <li>
                Euthanasia dates and countdown timers are <strong>estimates based on available shelter data</strong>.
                WaitingTheLongest.com does NOT independently verify euthanasia schedules and
                CANNOT guarantee their accuracy. A dog&apos;s actual status may change at any
                time without notice.
              </li>
              <li>
                Wait times are computed from shelter-reported intake dates and may be subject
                to capping, adjustment, or estimation where source data is incomplete.
              </li>
              <li>
                Dog availability, health status, temperament, photos, and other listing
                details are provided by shelters and rescue organizations.
                WaitingTheLongest.com does not verify the accuracy of individual listings.
              </li>
              <li>
                <strong>You must always contact the shelter or rescue directly</strong> to
                verify a dog&apos;s current availability, health, and status before making
                any decisions or taking any action.
              </li>
            </ul>
            <p className="mt-3">
              Each dog listing includes a{" "}
              <strong>Data Sources &amp; Verification</strong> page where you can review
              the specific sources, credibility score, and verification history for that
              listing.
            </p>
          </section>

          <section>
            <h2 className="text-lg font-semibold text-gray-900 mt-8 mb-3">
              4. No Warranty
            </h2>
            <p>
              THE PLATFORM IS PROVIDED &quot;AS IS&quot; AND &quot;AS AVAILABLE&quot; WITHOUT
              WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
              TO IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE,
              AND NON-INFRINGEMENT.
            </p>
            <p className="mt-2">
              We do not warrant that: (a) the Platform will be uninterrupted, secure,
              or error-free; (b) the data displayed will be accurate, complete, current,
              or reliable; (c) any dog listed on the Platform is still available, alive,
              or adoptable; or (d) the results obtained from the use of the Platform
              will be accurate or reliable.
            </p>
          </section>

          <section>
            <h2 className="text-lg font-semibold text-gray-900 mt-8 mb-3">
              5. Limitation of Liability
            </h2>
            <p>
              TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, IN NO EVENT SHALL
              WAITINGTHELONGEST.COM, ITS OPERATORS, EMPLOYEES, AGENTS, OR AFFILIATES
              BE LIABLE FOR ANY INDIRECT, INCIDENTAL, SPECIAL, CONSEQUENTIAL, OR PUNITIVE
              DAMAGES, INCLUDING WITHOUT LIMITATION LOSS OF PROFITS, DATA, USE, GOODWILL,
              OR OTHER INTANGIBLE LOSSES, ARISING OUT OF OR IN CONNECTION WITH:
            </p>
            <ul className="list-disc ml-6 mt-2 space-y-1">
              <li>Your use of or inability to use the Platform;</li>
              <li>Any inaccuracy in dog listing data, euthanasia dates, or wait times;</li>
              <li>Any adoption outcome, including but not limited to animal health issues, behavioral issues, or failed adoptions;</li>
              <li>Any action taken or not taken in reliance on information displayed on the Platform;</li>
              <li>The death of any animal listed on the Platform, whether or not the euthanasia date was displayed accurately.</li>
            </ul>
            <p className="mt-2">
              Our total liability for any claim arising out of or relating to these Terms
              or the Platform shall not exceed $100.
            </p>
          </section>

          <section>
            <h2 className="text-lg font-semibold text-gray-900 mt-8 mb-3">
              6. User Responsibilities
            </h2>
            <p>By using the Platform, you agree to:</p>
            <ul className="list-disc ml-6 mt-2 space-y-1">
              <li>Verify all information directly with shelters before taking action;</li>
              <li>Not rely solely on countdown timers or wait times for decision-making;</li>
              <li>Assume all risk associated with adoption outcomes;</li>
              <li>Not use the Platform for any unlawful purpose;</li>
              <li>Not attempt to scrape, harvest, or redistribute Platform data without authorization.</li>
            </ul>
          </section>

          <section>
            <h2 className="text-lg font-semibold text-gray-900 mt-8 mb-3">
              7. Intellectual Property
            </h2>
            <p>
              Dog photos and listing descriptions are the property of their respective
              shelters and rescue organizations. The Platform displays this content under
              fair use for nonprofit animal welfare purposes. If you believe content on
              the Platform infringes your copyright, please see our{" "}
              <Link href="/legal/dmca" className="text-blue-600 hover:underline">
                DMCA Policy
              </Link>.
            </p>
          </section>

          <section>
            <h2 className="text-lg font-semibold text-gray-900 mt-8 mb-3">
              8. Privacy
            </h2>
            <p>
              Your use of the Platform is also governed by our{" "}
              <Link href="/legal/privacy-policy" className="text-blue-600 hover:underline">
                Privacy Policy
              </Link>, which describes how we collect, use, and protect your information.
            </p>
          </section>

          <section>
            <h2 className="text-lg font-semibold text-gray-900 mt-8 mb-3">
              9. Modifications
            </h2>
            <p>
              We reserve the right to modify these Terms at any time. Changes will be
              posted on this page with an updated &quot;Last updated&quot; date. Continued use
              of the Platform after changes constitutes acceptance of the modified Terms.
            </p>
          </section>

          <section>
            <h2 className="text-lg font-semibold text-gray-900 mt-8 mb-3">
              10. Governing Law
            </h2>
            <p>
              These Terms shall be governed by and construed in accordance with the laws
              of the United States and the State of Texas, without regard to its
              conflict of law principles. Any dispute arising under these Terms shall be
              resolved in the courts located in the State of Texas.
            </p>
          </section>

          <section>
            <h2 className="text-lg font-semibold text-gray-900 mt-8 mb-3">
              11. Contact
            </h2>
            <p>
              For questions about these Terms, contact us at:{" "}
              <a href="mailto:legal@waitingthelongest.com" className="text-blue-600 hover:underline">
                legal@waitingthelongest.com
              </a>
            </p>
          </section>
        </div>

        <div className="mt-12 pt-6 border-t text-sm text-gray-500">
          <Link href="/legal/privacy-policy" className="hover:underline mr-6">
            Privacy Policy
          </Link>
          <Link href="/legal/dmca" className="hover:underline mr-6">
            DMCA Policy
          </Link>
          <Link href="/legal/shelter-terms" className="hover:underline mr-6">
            Shelter Terms
          </Link>
          <Link href="/legal/api-terms" className="hover:underline">
            API Terms
          </Link>
        </div>
      </div>
    </div>
  );
}
