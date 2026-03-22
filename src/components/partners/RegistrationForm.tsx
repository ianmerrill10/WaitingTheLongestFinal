"use client";

import { useState } from "react";
import Link from "next/link";

const US_STATES = [
  { code: "AL", name: "Alabama" }, { code: "AK", name: "Alaska" }, { code: "AZ", name: "Arizona" },
  { code: "AR", name: "Arkansas" }, { code: "CA", name: "California" }, { code: "CO", name: "Colorado" },
  { code: "CT", name: "Connecticut" }, { code: "DE", name: "Delaware" }, { code: "FL", name: "Florida" },
  { code: "GA", name: "Georgia" }, { code: "HI", name: "Hawaii" }, { code: "ID", name: "Idaho" },
  { code: "IL", name: "Illinois" }, { code: "IN", name: "Indiana" }, { code: "IA", name: "Iowa" },
  { code: "KS", name: "Kansas" }, { code: "KY", name: "Kentucky" }, { code: "LA", name: "Louisiana" },
  { code: "ME", name: "Maine" }, { code: "MD", name: "Maryland" }, { code: "MA", name: "Massachusetts" },
  { code: "MI", name: "Michigan" }, { code: "MN", name: "Minnesota" }, { code: "MS", name: "Mississippi" },
  { code: "MO", name: "Missouri" }, { code: "MT", name: "Montana" }, { code: "NE", name: "Nebraska" },
  { code: "NV", name: "Nevada" }, { code: "NH", name: "New Hampshire" }, { code: "NJ", name: "New Jersey" },
  { code: "NM", name: "New Mexico" }, { code: "NY", name: "New York" }, { code: "NC", name: "North Carolina" },
  { code: "ND", name: "North Dakota" }, { code: "OH", name: "Ohio" }, { code: "OK", name: "Oklahoma" },
  { code: "OR", name: "Oregon" }, { code: "PA", name: "Pennsylvania" }, { code: "RI", name: "Rhode Island" },
  { code: "SC", name: "South Carolina" }, { code: "SD", name: "South Dakota" }, { code: "TN", name: "Tennessee" },
  { code: "TX", name: "Texas" }, { code: "UT", name: "Utah" }, { code: "VT", name: "Vermont" },
  { code: "VA", name: "Virginia" }, { code: "WA", name: "Washington" }, { code: "WV", name: "West Virginia" },
  { code: "WI", name: "Wisconsin" }, { code: "WY", name: "Wyoming" }, { code: "DC", name: "District of Columbia" },
];

const ORG_TYPES = [
  { value: "municipal", label: "Municipal Shelter (City/County)" },
  { value: "private", label: "Private Shelter" },
  { value: "rescue", label: "Rescue Organization" },
  { value: "foster_network", label: "Foster-Based Network" },
  { value: "humane_society", label: "Humane Society" },
  { value: "spca", label: "SPCA" },
];

const SOFTWARE_OPTIONS = [
  "ShelterLuv",
  "PetPoint",
  "Chameleon",
  "RescueGroups.org",
  "Shelter Manager",
  "Pawlytics",
  "Petstablished",
  "Spreadsheet / Manual",
  "Custom / Other",
];

interface FormData {
  // Step 1: Organization
  org_name: string;
  org_type: string;
  ein: string;
  website: string;
  address: string;
  city: string;
  state_code: string;
  zip_code: string;
  // Step 2: Contact
  contact_first_name: string;
  contact_last_name: string;
  contact_email: string;
  contact_phone: string;
  contact_title: string;
  // Step 3: Integration
  estimated_dog_count: string;
  current_software: string;
  integration_preference: string;
  data_feed_url: string;
  // Step 4: Social
  social_facebook: string;
  social_instagram: string;
  social_tiktok: string;
  referral_source: string;
  // Step 5: Legal
  agree_terms: boolean;
  agree_data_sharing: boolean;
  agree_privacy: boolean;
}

const INITIAL_FORM: FormData = {
  org_name: "",
  org_type: "",
  ein: "",
  website: "",
  address: "",
  city: "",
  state_code: "",
  zip_code: "",
  contact_first_name: "",
  contact_last_name: "",
  contact_email: "",
  contact_phone: "",
  contact_title: "",
  estimated_dog_count: "",
  current_software: "",
  integration_preference: "api",
  data_feed_url: "",
  social_facebook: "",
  social_instagram: "",
  social_tiktok: "",
  referral_source: "",
  agree_terms: false,
  agree_data_sharing: false,
  agree_privacy: false,
};

const STEP_TITLES = [
  "Organization Info",
  "Contact Info",
  "Integration",
  "Social Media",
  "Legal Agreements",
];

export default function RegistrationForm() {
  const [step, setStep] = useState(1);
  const [form, setForm] = useState<FormData>(INITIAL_FORM);
  const [submitting, setSubmitting] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState(false);

  const updateField = (field: keyof FormData, value: string | boolean) => {
    setForm((prev) => ({ ...prev, [field]: value }));
    setError(null);
  };

  const validateStep = (): string | null => {
    switch (step) {
      case 1:
        if (!form.org_name.trim()) return "Organization name is required";
        if (!form.org_type) return "Organization type is required";
        if (!form.city.trim()) return "City is required";
        if (!form.state_code) return "State is required";
        if (!form.zip_code.trim()) return "ZIP code is required";
        if (form.zip_code.trim().length < 5) return "Enter a valid ZIP code";
        return null;
      case 2:
        if (!form.contact_first_name.trim()) return "First name is required";
        if (!form.contact_last_name.trim()) return "Last name is required";
        if (!form.contact_email.trim()) return "Email is required";
        if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(form.contact_email))
          return "Enter a valid email address";
        return null;
      case 3:
        return null; // All optional
      case 4:
        return null; // All optional
      case 5:
        if (!form.agree_terms)
          return "You must accept the Terms of Service";
        if (!form.agree_data_sharing)
          return "You must accept the Data Sharing Agreement";
        if (!form.agree_privacy)
          return "You must accept the Privacy Policy";
        return null;
      default:
        return null;
    }
  };

  const nextStep = () => {
    const err = validateStep();
    if (err) {
      setError(err);
      return;
    }
    setStep((s) => Math.min(s + 1, 5));
  };

  const prevStep = () => setStep((s) => Math.max(s - 1, 1));

  const handleSubmit = async () => {
    const err = validateStep();
    if (err) {
      setError(err);
      return;
    }

    setSubmitting(true);
    setError(null);

    try {
      const res = await fetch("/api/partners/register", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          org_name: form.org_name.trim(),
          org_type: form.org_type,
          ein: form.ein.trim() || undefined,
          website: form.website.trim() || undefined,
          address: form.address.trim() || undefined,
          city: form.city.trim(),
          state_code: form.state_code,
          zip_code: form.zip_code.trim(),
          contact_first_name: form.contact_first_name.trim(),
          contact_last_name: form.contact_last_name.trim(),
          contact_email: form.contact_email.trim().toLowerCase(),
          contact_phone: form.contact_phone.trim() || undefined,
          contact_title: form.contact_title.trim() || undefined,
          estimated_dog_count: form.estimated_dog_count
            ? parseInt(form.estimated_dog_count)
            : undefined,
          current_software: form.current_software || undefined,
          integration_preference: form.integration_preference,
          data_feed_url: form.data_feed_url.trim() || undefined,
          social_facebook: form.social_facebook.trim() || undefined,
          social_instagram: form.social_instagram.trim() || undefined,
          social_tiktok: form.social_tiktok.trim() || undefined,
          referral_source: form.referral_source.trim() || undefined,
        }),
      });

      const data = await res.json();

      if (!res.ok) {
        setError(data.error || "Registration failed. Please try again.");
        return;
      }

      setSuccess(true);
    } catch {
      setError("Network error. Please check your connection and try again.");
    } finally {
      setSubmitting(false);
    }
  };

  if (success) {
    return (
      <div className="max-w-2xl mx-auto text-center py-12">
        <div className="w-20 h-20 bg-green-100 rounded-full flex items-center justify-center mx-auto mb-6">
          <svg className="w-10 h-10 text-green-600" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 13l4 4L19 7" />
          </svg>
        </div>
        <h2 className="text-3xl font-bold mb-4">Registration Submitted!</h2>
        <p className="text-lg text-gray-600 mb-6">
          Thank you for registering <strong>{form.org_name}</strong>. We&apos;ve
          received your application and will review it shortly.
        </p>
        <div className="bg-green-50 border border-green-200 rounded-lg p-6 text-left mb-8">
          <h3 className="font-bold text-green-800 mb-3">What happens next?</h3>
          <ol className="list-decimal list-inside space-y-2 text-green-700">
            <li>We&apos;ll review your application (typically within 24-48 hours)</li>
            <li>You&apos;ll receive a confirmation email at <strong>{form.contact_email}</strong></li>
            <li>We&apos;ll match your organization against our database and set up your integration</li>
            <li>Your dogs will go live with real-time wait counters</li>
          </ol>
        </div>
        <Link
          href="/"
          className="text-green-700 hover:text-green-800 font-semibold"
        >
          Return to Homepage
        </Link>
      </div>
    );
  }

  return (
    <div className="max-w-3xl mx-auto">
      {/* Progress Bar */}
      <div className="mb-8">
        <div className="flex justify-between mb-2">
          {STEP_TITLES.map((title, i) => (
            <div
              key={title}
              className={`text-xs font-medium ${
                i + 1 <= step ? "text-green-700" : "text-gray-400"
              } ${i + 1 === step ? "font-bold" : ""}`}
            >
              <span className="hidden sm:inline">{title}</span>
              <span className="sm:hidden">{i + 1}</span>
            </div>
          ))}
        </div>
        <div className="w-full bg-gray-200 rounded-full h-2">
          <div
            className="bg-green-600 h-2 rounded-full transition-all duration-300"
            style={{ width: `${(step / 5) * 100}%` }}
          />
        </div>
      </div>

      {/* Step Content */}
      <div className="bg-white rounded-xl shadow-sm border border-gray-200 p-6 md:p-8">
        <h2 className="text-xl font-bold mb-6">
          Step {step}: {STEP_TITLES[step - 1]}
        </h2>

        {step === 1 && (
          <div className="space-y-4">
            <Field label="Organization Name" required>
              <input
                type="text"
                value={form.org_name}
                onChange={(e) => updateField("org_name", e.target.value)}
                placeholder="Happy Paws Animal Shelter"
                className="form-input"
              />
            </Field>
            <Field label="Organization Type" required>
              <select
                value={form.org_type}
                onChange={(e) => updateField("org_type", e.target.value)}
                className="form-input"
              >
                <option value="">Select type...</option>
                {ORG_TYPES.map((t) => (
                  <option key={t.value} value={t.value}>
                    {t.label}
                  </option>
                ))}
              </select>
            </Field>
            <Field label="EIN (Tax ID)" hint="For nonprofit verification">
              <input
                type="text"
                value={form.ein}
                onChange={(e) => updateField("ein", e.target.value)}
                placeholder="XX-XXXXXXX"
                className="form-input"
              />
            </Field>
            <Field label="Website">
              <input
                type="url"
                value={form.website}
                onChange={(e) => updateField("website", e.target.value)}
                placeholder="https://www.yourshelter.org"
                className="form-input"
              />
            </Field>
            <Field label="Street Address">
              <input
                type="text"
                value={form.address}
                onChange={(e) => updateField("address", e.target.value)}
                placeholder="123 Main Street"
                className="form-input"
              />
            </Field>
            <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
              <div className="col-span-2 md:col-span-1">
                <Field label="City" required>
                  <input
                    type="text"
                    value={form.city}
                    onChange={(e) => updateField("city", e.target.value)}
                    className="form-input"
                  />
                </Field>
              </div>
              <Field label="State" required>
                <select
                  value={form.state_code}
                  onChange={(e) => updateField("state_code", e.target.value)}
                  className="form-input"
                >
                  <option value="">State</option>
                  {US_STATES.map((s) => (
                    <option key={s.code} value={s.code}>
                      {s.code}
                    </option>
                  ))}
                </select>
              </Field>
              <Field label="ZIP" required>
                <input
                  type="text"
                  value={form.zip_code}
                  onChange={(e) => updateField("zip_code", e.target.value)}
                  maxLength={10}
                  className="form-input"
                />
              </Field>
            </div>
          </div>
        )}

        {step === 2 && (
          <div className="space-y-4">
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              <Field label="First Name" required>
                <input
                  type="text"
                  value={form.contact_first_name}
                  onChange={(e) =>
                    updateField("contact_first_name", e.target.value)
                  }
                  className="form-input"
                />
              </Field>
              <Field label="Last Name" required>
                <input
                  type="text"
                  value={form.contact_last_name}
                  onChange={(e) =>
                    updateField("contact_last_name", e.target.value)
                  }
                  className="form-input"
                />
              </Field>
            </div>
            <Field label="Email Address" required>
              <input
                type="email"
                value={form.contact_email}
                onChange={(e) => updateField("contact_email", e.target.value)}
                placeholder="you@yourshelter.org"
                className="form-input"
              />
            </Field>
            <Field label="Phone Number">
              <input
                type="tel"
                value={form.contact_phone}
                onChange={(e) => updateField("contact_phone", e.target.value)}
                placeholder="(555) 123-4567"
                className="form-input"
              />
            </Field>
            <Field label="Title / Role">
              <input
                type="text"
                value={form.contact_title}
                onChange={(e) => updateField("contact_title", e.target.value)}
                placeholder="Director, Intake Coordinator, etc."
                className="form-input"
              />
            </Field>
          </div>
        )}

        {step === 3 && (
          <div className="space-y-4">
            <Field
              label="Estimated Number of Dogs"
              hint="How many dogs do you typically have available at a time?"
            >
              <input
                type="number"
                value={form.estimated_dog_count}
                onChange={(e) =>
                  updateField("estimated_dog_count", e.target.value)
                }
                min={0}
                placeholder="25"
                className="form-input"
              />
            </Field>
            <Field label="Current Shelter Software">
              <select
                value={form.current_software}
                onChange={(e) =>
                  updateField("current_software", e.target.value)
                }
                className="form-input"
              >
                <option value="">Select software...</option>
                {SOFTWARE_OPTIONS.map((s) => (
                  <option key={s} value={s}>
                    {s}
                  </option>
                ))}
              </select>
            </Field>
            <Field label="Preferred Integration Method">
              <select
                value={form.integration_preference}
                onChange={(e) =>
                  updateField("integration_preference", e.target.value)
                }
                className="form-input"
              >
                <option value="api">REST API</option>
                <option value="webhook">Webhooks</option>
                <option value="feed">Feed URL (JSON/CSV)</option>
                <option value="csv_upload">CSV Upload</option>
                <option value="manual">Manual Entry</option>
              </select>
            </Field>
            <Field
              label="Public Data Feed URL"
              hint="If you have an existing public JSON or CSV feed of your animals"
            >
              <input
                type="url"
                value={form.data_feed_url}
                onChange={(e) => updateField("data_feed_url", e.target.value)}
                placeholder="https://yourshelter.org/api/animals.json"
                className="form-input"
              />
            </Field>
          </div>
        )}

        {step === 4 && (
          <div className="space-y-4">
            <p className="text-gray-600 mb-4">
              Help us promote your dogs! Provide your social media pages and
              we can tag you when featuring your animals.
            </p>
            <Field label="Facebook Page">
              <input
                type="url"
                value={form.social_facebook}
                onChange={(e) =>
                  updateField("social_facebook", e.target.value)
                }
                placeholder="https://facebook.com/yourshelter"
                className="form-input"
              />
            </Field>
            <Field label="Instagram">
              <input
                type="text"
                value={form.social_instagram}
                onChange={(e) =>
                  updateField("social_instagram", e.target.value)
                }
                placeholder="@yourshelter"
                className="form-input"
              />
            </Field>
            <Field label="TikTok">
              <input
                type="text"
                value={form.social_tiktok}
                onChange={(e) => updateField("social_tiktok", e.target.value)}
                placeholder="@yourshelter"
                className="form-input"
              />
            </Field>
            <Field label="How did you hear about us?">
              <input
                type="text"
                value={form.referral_source}
                onChange={(e) =>
                  updateField("referral_source", e.target.value)
                }
                placeholder="Social media, another shelter, web search..."
                className="form-input"
              />
            </Field>
          </div>
        )}

        {step === 5 && (
          <div className="space-y-6">
            <p className="text-gray-600">
              Please review and accept the following agreements to complete
              your registration.
            </p>
            <Checkbox
              checked={form.agree_terms}
              onChange={(v) => updateField("agree_terms", v)}
              label={
                <>
                  I accept the{" "}
                  <Link
                    href="/legal/shelter-terms"
                    target="_blank"
                    className="text-green-700 underline hover:text-green-800"
                  >
                    Terms of Service
                  </Link>{" "}
                  for shelter partners.
                </>
              }
            />
            <Checkbox
              checked={form.agree_data_sharing}
              onChange={(v) => updateField("agree_data_sharing", v)}
              label={
                <>
                  I accept the{" "}
                  <Link
                    href="/legal/data-sharing"
                    target="_blank"
                    className="text-green-700 underline hover:text-green-800"
                  >
                    Data Sharing Agreement
                  </Link>
                  .
                </>
              }
            />
            <Checkbox
              checked={form.agree_privacy}
              onChange={(v) => updateField("agree_privacy", v)}
              label={
                <>
                  I have read and agree to the{" "}
                  <Link
                    href="/legal/privacy-policy"
                    target="_blank"
                    className="text-green-700 underline hover:text-green-800"
                  >
                    Privacy Policy
                  </Link>
                  .
                </>
              }
            />
          </div>
        )}

        {/* Error Message */}
        {error && (
          <div className="mt-4 bg-red-50 border border-red-200 rounded-lg p-4 text-red-700 text-sm">
            {error}
          </div>
        )}

        {/* Navigation */}
        <div className="flex justify-between mt-8 pt-6 border-t border-gray-200">
          {step > 1 ? (
            <button
              onClick={prevStep}
              className="px-6 py-2.5 text-gray-700 border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors"
            >
              Back
            </button>
          ) : (
            <div />
          )}
          {step < 5 ? (
            <button
              onClick={nextStep}
              className="px-8 py-2.5 bg-green-700 text-white rounded-lg hover:bg-green-800 transition-colors font-semibold"
            >
              Continue
            </button>
          ) : (
            <button
              onClick={handleSubmit}
              disabled={submitting}
              className="px-8 py-2.5 bg-green-700 text-white rounded-lg hover:bg-green-800 transition-colors font-semibold disabled:opacity-50 disabled:cursor-not-allowed"
            >
              {submitting ? "Submitting..." : "Submit Registration"}
            </button>
          )}
        </div>
      </div>
    </div>
  );
}

function Field({
  label,
  required,
  hint,
  children,
}: {
  label: string;
  required?: boolean;
  hint?: string;
  children: React.ReactNode;
}) {
  return (
    <div>
      <label className="block text-sm font-medium text-gray-700 mb-1">
        {label}
        {required && <span className="text-red-500 ml-0.5">*</span>}
      </label>
      {hint && <p className="text-xs text-gray-500 mb-1">{hint}</p>}
      {children}
    </div>
  );
}

function Checkbox({
  checked,
  onChange,
  label,
}: {
  checked: boolean;
  onChange: (v: boolean) => void;
  label: React.ReactNode;
}) {
  return (
    <label className="flex items-start gap-3 cursor-pointer group">
      <input
        type="checkbox"
        checked={checked}
        onChange={(e) => onChange(e.target.checked)}
        className="mt-1 w-4 h-4 rounded border-gray-300 text-green-600 focus:ring-green-500"
      />
      <span className="text-sm text-gray-700 group-hover:text-gray-900">
        {label}
      </span>
    </label>
  );
}
