/**
 * Parse CSV data into dog records for bulk import.
 * Handles common CSV quirks: quoted fields, embedded commas, varied headers.
 */

export interface ParsedDogRow {
  name: string;
  breed: string | null;
  age_text: string | null;
  sex: string | null;
  size: string | null;
  color: string | null;
  weight: number | null;
  description: string | null;
  photo_url: string | null;
  additional_photos: string[];
  status: string;
  intake_date: string | null;
  adoption_fee: number | null;
  is_house_trained: boolean | null;
  good_with_kids: boolean | null;
  good_with_dogs: boolean | null;
  good_with_cats: boolean | null;
  special_needs: string | null;
  medical_notes: string | null;
  external_id: string | null;
  external_url: string | null;
}

export interface CsvParseResult {
  dogs: ParsedDogRow[];
  errors: Array<{ row: number; field: string; message: string }>;
  warnings: Array<{ row: number; field: string; message: string }>;
  total_rows: number;
  valid_rows: number;
}

// Column name aliases for flexible mapping
const COLUMN_ALIASES: Record<string, keyof ParsedDogRow> = {
  name: "name",
  dog_name: "name",
  "dog name": "name",
  pet_name: "name",
  animal_name: "name",
  breed: "breed",
  breed_primary: "breed",
  primary_breed: "breed",
  "breed/mix": "breed",
  age: "age_text",
  age_text: "age_text",
  age_string: "age_text",
  sex: "sex",
  gender: "sex",
  size: "size",
  size_general: "size",
  color: "color",
  color_primary: "color",
  weight: "weight",
  weight_lbs: "weight",
  description: "description",
  bio: "description",
  about: "description",
  photo: "photo_url",
  photo_url: "photo_url",
  primary_photo: "photo_url",
  image: "photo_url",
  image_url: "photo_url",
  additional_photos: "additional_photos",
  photos: "additional_photos",
  status: "status",
  availability: "status",
  intake_date: "intake_date",
  date_added: "intake_date",
  date_in: "intake_date",
  admission_date: "intake_date",
  fee: "adoption_fee",
  adoption_fee: "adoption_fee",
  adoption_cost: "adoption_fee",
  house_trained: "is_house_trained",
  housetrained: "is_house_trained",
  housebroken: "is_house_trained",
  good_with_kids: "good_with_kids",
  kids: "good_with_kids",
  children: "good_with_kids",
  good_with_dogs: "good_with_dogs",
  dogs: "good_with_dogs",
  other_dogs: "good_with_dogs",
  good_with_cats: "good_with_cats",
  cats: "good_with_cats",
  special_needs: "special_needs",
  medical: "medical_notes",
  medical_notes: "medical_notes",
  health: "medical_notes",
  external_id: "external_id",
  id: "external_id",
  animal_id: "external_id",
  external_url: "external_url",
  url: "external_url",
  link: "external_url",
};

/**
 * Parse raw CSV text into dog records.
 */
export function parseCsvDogs(csvText: string): CsvParseResult {
  const lines = parseCsvLines(csvText);
  if (lines.length < 2) {
    return {
      dogs: [],
      errors: [{ row: 0, field: "", message: "CSV must have at least a header row and one data row" }],
      warnings: [],
      total_rows: 0,
      valid_rows: 0,
    };
  }

  // Map headers
  const headers = lines[0].map((h) => h.toLowerCase().trim());
  const columnMap = new Map<number, keyof ParsedDogRow>();

  for (let i = 0; i < headers.length; i++) {
    const alias = COLUMN_ALIASES[headers[i]];
    if (alias) {
      columnMap.set(i, alias);
    }
  }

  // Check for required name column
  const hasName = Array.from(columnMap.values()).includes("name");
  if (!hasName) {
    return {
      dogs: [],
      errors: [{ row: 0, field: "name", message: "CSV must contain a 'name' or 'dog_name' column" }],
      warnings: [],
      total_rows: lines.length - 1,
      valid_rows: 0,
    };
  }

  const dogs: ParsedDogRow[] = [];
  const errors: CsvParseResult["errors"] = [];
  const warnings: CsvParseResult["warnings"] = [];

  for (let rowIdx = 1; rowIdx < lines.length; rowIdx++) {
    const row = lines[rowIdx];
    if (row.length === 0 || (row.length === 1 && !row[0].trim())) continue;

    const dog: Partial<ParsedDogRow> = {
      additional_photos: [],
      status: "available",
    };

    for (const [colIdx, field] of Array.from(columnMap.entries())) {
      const value = row[colIdx]?.trim() || "";
      if (!value) continue;

      switch (field) {
        case "weight":
        case "adoption_fee": {
          const num = parseFloat(value.replace(/[^0-9.]/g, ""));
          if (!isNaN(num)) (dog as Record<string, unknown>)[field] = num;
          break;
        }
        case "is_house_trained":
        case "good_with_kids":
        case "good_with_dogs":
        case "good_with_cats":
          (dog as Record<string, unknown>)[field] = parseBoolean(value);
          break;
        case "additional_photos":
          dog.additional_photos = value
            .split(/[,;|]/)
            .map((u) => u.trim())
            .filter(Boolean);
          break;
        case "sex":
          dog.sex = normalizeSex(value);
          break;
        case "size":
          dog.size = normalizeSize(value);
          break;
        case "status":
          dog.status = normalizeStatus(value);
          break;
        default:
          (dog as Record<string, unknown>)[field] = value;
      }
    }

    // Validate required fields
    if (!dog.name) {
      errors.push({ row: rowIdx + 1, field: "name", message: "Name is required" });
      continue;
    }

    // Warnings for missing but recommended fields
    if (!dog.breed) {
      warnings.push({ row: rowIdx + 1, field: "breed", message: "Breed not provided" });
    }
    if (!dog.photo_url) {
      warnings.push({ row: rowIdx + 1, field: "photo", message: "No photo URL provided" });
    }

    dogs.push({
      name: dog.name!,
      breed: dog.breed || null,
      age_text: dog.age_text || null,
      sex: dog.sex || null,
      size: dog.size || null,
      color: dog.color || null,
      weight: dog.weight || null,
      description: dog.description || null,
      photo_url: dog.photo_url || null,
      additional_photos: dog.additional_photos || [],
      status: dog.status || "available",
      intake_date: dog.intake_date || null,
      adoption_fee: dog.adoption_fee || null,
      is_house_trained: dog.is_house_trained ?? null,
      good_with_kids: dog.good_with_kids ?? null,
      good_with_dogs: dog.good_with_dogs ?? null,
      good_with_cats: dog.good_with_cats ?? null,
      special_needs: dog.special_needs || null,
      medical_notes: dog.medical_notes || null,
      external_id: dog.external_id || null,
      external_url: dog.external_url || null,
    });
  }

  return {
    dogs,
    errors,
    warnings,
    total_rows: lines.length - 1,
    valid_rows: dogs.length,
  };
}

/**
 * Parse CSV text into array of arrays, handling quoted fields.
 */
function parseCsvLines(text: string): string[][] {
  const lines: string[][] = [];
  let current: string[] = [];
  let field = "";
  let inQuotes = false;

  for (let i = 0; i < text.length; i++) {
    const ch = text[i];

    if (inQuotes) {
      if (ch === '"') {
        if (text[i + 1] === '"') {
          field += '"';
          i++;
        } else {
          inQuotes = false;
        }
      } else {
        field += ch;
      }
    } else {
      if (ch === '"') {
        inQuotes = true;
      } else if (ch === ",") {
        current.push(field);
        field = "";
      } else if (ch === "\n" || ch === "\r") {
        if (ch === "\r" && text[i + 1] === "\n") i++;
        current.push(field);
        field = "";
        if (current.some((f) => f.trim())) lines.push(current);
        current = [];
      } else {
        field += ch;
      }
    }
  }

  // Last field/line
  current.push(field);
  if (current.some((f) => f.trim())) lines.push(current);

  return lines;
}

function parseBoolean(val: string): boolean | null {
  const lower = val.toLowerCase();
  if (["yes", "true", "1", "y"].includes(lower)) return true;
  if (["no", "false", "0", "n"].includes(lower)) return false;
  if (["unknown", "n/a", ""].includes(lower)) return null;
  return null;
}

function normalizeSex(val: string): string {
  const lower = val.toLowerCase();
  if (lower.startsWith("m")) return "Male";
  if (lower.startsWith("f")) return "Female";
  return val;
}

function normalizeSize(val: string): string {
  const lower = val.toLowerCase();
  if (lower.includes("small") || lower === "s") return "Small";
  if (lower.includes("medium") || lower === "m") return "Medium";
  if (lower.includes("large") && !lower.includes("x")) return "Large";
  if (lower.includes("xlarge") || lower.includes("x-large") || lower.includes("extra")) return "X-Large";
  return val;
}

function normalizeStatus(val: string): string {
  const lower = val.toLowerCase();
  if (lower.includes("available") || lower.includes("adoptable")) return "available";
  if (lower.includes("adopted")) return "adopted";
  if (lower.includes("hold")) return "hold";
  if (lower.includes("pending")) return "pending";
  if (lower.includes("transfer")) return "transferred";
  return "available";
}
