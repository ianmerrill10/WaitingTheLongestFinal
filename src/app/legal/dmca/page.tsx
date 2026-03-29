import type { Metadata } from "next";
import Link from "next/link";

export const metadata: Metadata = {
  title: "DMCA Policy | WaitingTheLongest.com",
  description: "DMCA takedown request procedure for WaitingTheLongest.com.",
};

export default function DMCAPage() {
  return (
    <div className="min-h-screen bg-white">
      <div className="max-w-3xl mx-auto px-4 py-12">
        <h1 className="text-3xl font-bold text-gray-900 mb-2">DMCA Policy</h1>
        <p className="text-sm text-gray-500 mb-8">Last updated: March 25, 2026</p>

        <div className="prose prose-gray max-w-none space-y-6 text-gray-700 text-sm leading-relaxed">
          <section>
            <h2 className="text-lg font-semibold text-gray-900 mt-8 mb-3">
              1. Overview
            </h2>
            <p>
              WaitingTheLongest.com respects the intellectual property rights of
              others. We display dog photos, descriptions, and listing information
              sourced from shelters, rescue organizations, and third-party databases
              for the purpose of promoting animal adoption and welfare.
            </p>
            <p className="mt-2">
              If you believe that content on our Platform infringes your copyright,
              you may submit a DMCA takedown notice as described below.
            </p>
          </section>

          <section>
            <h2 className="text-lg font-semibold text-gray-900 mt-8 mb-3">
              2. Filing a DMCA Takedown Notice
            </h2>
            <p>
              To submit a valid DMCA takedown notice, please provide the following
              information in writing:
            </p>
            <ol className="list-decimal ml-6 mt-2 space-y-2">
              <li>
                A physical or electronic signature of the copyright owner or a person
                authorized to act on their behalf;
              </li>
              <li>
                Identification of the copyrighted work claimed to have been infringed;
              </li>
              <li>
                Identification of the material that is claimed to be infringing,
                including the URL(s) where the material appears on our Platform;
              </li>
              <li>
                Your contact information, including name, address, telephone number,
                and email address;
              </li>
              <li>
                A statement that you have a good faith belief that use of the material
                is not authorized by the copyright owner, its agent, or the law;
              </li>
              <li>
                A statement, made under penalty of perjury, that the information in
                the notification is accurate and that you are authorized to act on
                behalf of the copyright owner.
              </li>
            </ol>
          </section>

          <section>
            <h2 className="text-lg font-semibold text-gray-900 mt-8 mb-3">
              3. Where to Send Notices
            </h2>
            <p>
              Send DMCA takedown notices to:
            </p>
            <div className="bg-gray-50 rounded-lg p-4 mt-2">
              <p className="font-medium">DMCA Agent</p>
              <p>WaitingTheLongest.com</p>
              <p>
                Email:{" "}
                <a
                  href="mailto:legal@waitingthelongest.com"
                  className="text-blue-600 hover:underline"
                >
                  legal@waitingthelongest.com
                </a>
              </p>
              <p className="mt-2 text-xs text-gray-500">
                Please include &quot;DMCA Takedown&quot; in the subject line.
              </p>
            </div>
          </section>

          <section>
            <h2 className="text-lg font-semibold text-gray-900 mt-8 mb-3">
              4. Response Timeline
            </h2>
            <p>
              Upon receipt of a valid DMCA takedown notice, we will:
            </p>
            <ul className="list-disc ml-6 mt-2 space-y-1">
              <li>Acknowledge receipt within 24 hours;</li>
              <li>Remove or disable access to the claimed infringing material within 48-72 hours;</li>
              <li>Notify the content provider (if applicable) of the takedown.</li>
            </ul>
          </section>

          <section>
            <h2 className="text-lg font-semibold text-gray-900 mt-8 mb-3">
              5. Counter-Notification
            </h2>
            <p>
              If you believe that content was removed in error, you may submit a
              counter-notification containing:
            </p>
            <ol className="list-decimal ml-6 mt-2 space-y-2">
              <li>Your physical or electronic signature;</li>
              <li>Identification of the removed material and its former location;</li>
              <li>
                A statement under penalty of perjury that you have a good faith belief
                the material was removed by mistake or misidentification;
              </li>
              <li>Your name, address, and telephone number;</li>
              <li>
                Consent to jurisdiction of the federal court in your district (or the
                Southern District of Texas if outside the US).
              </li>
            </ol>
          </section>

          <section>
            <h2 className="text-lg font-semibold text-gray-900 mt-8 mb-3">
              6. Repeat Infringer Policy
            </h2>
            <p>
              In accordance with the DMCA, we will terminate access for users or
              content providers who are repeat infringers in appropriate circumstances.
            </p>
          </section>
        </div>

        <div className="mt-12 pt-6 border-t text-sm text-gray-500">
          <Link href="/legal/terms-of-service" className="hover:underline mr-6">
            Terms of Service
          </Link>
          <Link href="/legal/privacy-policy" className="hover:underline mr-6">
            Privacy Policy
          </Link>
          <Link href="/legal/shelter-terms" className="hover:underline">
            Shelter Terms
          </Link>
        </div>
      </div>
    </div>
  );
}
