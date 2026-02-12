"""
Build Debug Agent - Error categorizer.

Classifies parsed BuildErrors into the 13 defined categories using
regex pattern matching against error messages.
"""

import re
from .models import BuildError, BuildErrorCategory

# Category detection rules: (compiled_regex, category)
# Checked in order - first match wins
CATEGORY_RULES: list[tuple[re.Pattern, BuildErrorCategory]] = [
    # MISSING_HEADER: file not found, no such file
    (re.compile(r"No such file or directory", re.I), BuildErrorCategory.MISSING_HEADER),
    (re.compile(r"file not found", re.I), BuildErrorCategory.MISSING_HEADER),
    (re.compile(r"#include.*not found", re.I), BuildErrorCategory.MISSING_HEADER),

    # UNDEFINED_REFERENCE: linker can't find symbol
    (re.compile(r"undefined reference to"), BuildErrorCategory.UNDEFINED_REFERENCE),
    (re.compile(r"unresolved external symbol"), BuildErrorCategory.UNDEFINED_REFERENCE),

    # DUPLICATE_SYMBOL: multiple definitions
    (re.compile(r"multiple definition of"), BuildErrorCategory.DUPLICATE_SYMBOL),
    (re.compile(r"redefinition of"), BuildErrorCategory.DUPLICATE_SYMBOL),
    (re.compile(r"previously defined here"), BuildErrorCategory.DUPLICATE_SYMBOL),
    (re.compile(r"already defined in"), BuildErrorCategory.DUPLICATE_SYMBOL),

    # ACCESS_ERROR: private/protected member access
    (re.compile(r"is private within this context"), BuildErrorCategory.ACCESS_ERROR),
    (re.compile(r"is a private member of"), BuildErrorCategory.ACCESS_ERROR),
    (re.compile(r"is protected within this context"), BuildErrorCategory.ACCESS_ERROR),
    (re.compile(r"is a protected member of"), BuildErrorCategory.ACCESS_ERROR),
    (re.compile(r"inaccessible"), BuildErrorCategory.ACCESS_ERROR),

    # IMPLICIT_CONVERSION: narrowing conversions
    (re.compile(r"narrowing conversion"), BuildErrorCategory.IMPLICIT_CONVERSION),
    (re.compile(r"implicit conversion loses"), BuildErrorCategory.IMPLICIT_CONVERSION),
    (re.compile(r"truncation of constant value"), BuildErrorCategory.IMPLICIT_CONVERSION),

    # TEMPLATE_ERROR: template instantiation issues
    (re.compile(r"in instantiation of"), BuildErrorCategory.TEMPLATE_ERROR),
    (re.compile(r"required from"), BuildErrorCategory.TEMPLATE_ERROR),
    (re.compile(r"template argument deduction"), BuildErrorCategory.TEMPLATE_ERROR),
    (re.compile(r"candidate template ignored"), BuildErrorCategory.TEMPLATE_ERROR),
    (re.compile(r"no type named.*in.*template"), BuildErrorCategory.TEMPLATE_ERROR),

    # TYPE_MISMATCH: type conversion errors
    (re.compile(r"cannot convert"), BuildErrorCategory.TYPE_MISMATCH),
    (re.compile(r"no matching function for call"), BuildErrorCategory.TYPE_MISMATCH),
    (re.compile(r"invalid conversion from"), BuildErrorCategory.TYPE_MISMATCH),
    (re.compile(r"no viable conversion"), BuildErrorCategory.TYPE_MISMATCH),
    (re.compile(r"incompatible type"), BuildErrorCategory.TYPE_MISMATCH),
    (re.compile(r"cannot initialize.*with"), BuildErrorCategory.TYPE_MISMATCH),
    (re.compile(r"no matching constructor"), BuildErrorCategory.TYPE_MISMATCH),

    # MISSING_METHOD: class member not found
    (re.compile(r"has no member named"), BuildErrorCategory.MISSING_METHOD),
    (re.compile(r"no member named"), BuildErrorCategory.MISSING_METHOD),
    (re.compile(r"is not a member function"), BuildErrorCategory.MISSING_METHOD),

    # NAMESPACE_ERROR: undeclared identifiers, wrong scope
    (re.compile(r"was not declared in this scope"), BuildErrorCategory.NAMESPACE_ERROR),
    (re.compile(r"is not a member of"), BuildErrorCategory.NAMESPACE_ERROR),
    (re.compile(r"use of undeclared identifier"), BuildErrorCategory.NAMESPACE_ERROR),
    (re.compile(r"does not name a type"), BuildErrorCategory.NAMESPACE_ERROR),
    (re.compile(r"unknown type name"), BuildErrorCategory.NAMESPACE_ERROR),
    (re.compile(r"no type named"), BuildErrorCategory.NAMESPACE_ERROR),
    (re.compile(r"undeclared identifier"), BuildErrorCategory.NAMESPACE_ERROR),

    # SYNTAX_ERROR: syntax issues
    (re.compile(r"expected ['\"]?[;{})\]>]"), BuildErrorCategory.SYNTAX_ERROR),
    (re.compile(r"expected primary-expression"), BuildErrorCategory.SYNTAX_ERROR),
    (re.compile(r"expected declaration"), BuildErrorCategory.SYNTAX_ERROR),
    (re.compile(r"expected unqualified-id"), BuildErrorCategory.SYNTAX_ERROR),
    (re.compile(r"stray .* in program"), BuildErrorCategory.SYNTAX_ERROR),
    (re.compile(r"missing terminating"), BuildErrorCategory.SYNTAX_ERROR),
    (re.compile(r"unterminated"), BuildErrorCategory.SYNTAX_ERROR),
]


class ErrorCategorizer:
    """Classifies BuildErrors into defined categories."""

    def categorize(self, error: BuildError) -> BuildErrorCategory:
        """Classify a single error by matching message against rules."""
        # Skip if already categorized (e.g., linker errors from parser)
        if error.category != BuildErrorCategory.UNKNOWN:
            return error.category

        for pattern, category in CATEGORY_RULES:
            if pattern.search(error.message):
                return category

        return BuildErrorCategory.UNKNOWN

    def categorize_batch(self, errors: list[BuildError]) -> list[BuildError]:
        """Classify all errors in a batch, mutating category in place."""
        for error in errors:
            error.category = self.categorize(error)

        # Detect related errors
        self._detect_related_errors(errors)

        return errors

    def _detect_related_errors(self, errors: list[BuildError]) -> None:
        """Link errors that share the same file or are in the same include chain."""
        # Group by file
        by_file: dict[str, list[BuildError]] = {}
        for error in errors:
            by_file.setdefault(error.file, []).append(error)

        # Errors in the same file within 5 lines are likely related
        for file_errors in by_file.values():
            sorted_errors = sorted(file_errors, key=lambda e: e.line)
            for i in range(len(sorted_errors)):
                for j in range(i + 1, len(sorted_errors)):
                    if sorted_errors[j].line - sorted_errors[i].line <= 5:
                        if sorted_errors[j].id not in sorted_errors[i].related_errors:
                            sorted_errors[i].related_errors.append(sorted_errors[j].id)
                        if sorted_errors[i].id not in sorted_errors[j].related_errors:
                            sorted_errors[j].related_errors.append(sorted_errors[i].id)
                    else:
                        break

        # MISSING_HEADER errors cascade - mark downstream errors
        missing_header_files = set()
        for error in errors:
            if error.category == BuildErrorCategory.MISSING_HEADER:
                missing_header_files.add(error.file)

        for error in errors:
            if error.file in missing_header_files and error.category != BuildErrorCategory.MISSING_HEADER:
                error.metadata["possibly_cascade"] = True
                error.metadata["cascade_from"] = "MISSING_HEADER in same file"

    def get_cascade_estimate(self, errors: list[BuildError]) -> dict:
        """Estimate how many errors are caused by cascading from header errors."""
        missing_header_files = set()
        for error in errors:
            if error.category == BuildErrorCategory.MISSING_HEADER:
                missing_header_files.add(error.file)

        cascade_count = sum(
            1
            for e in errors
            if e.file in missing_header_files
            and e.category != BuildErrorCategory.MISSING_HEADER
        )

        return {
            "missing_header_count": len(
                [e for e in errors if e.category == BuildErrorCategory.MISSING_HEADER]
            ),
            "files_with_missing_headers": len(missing_header_files),
            "estimated_cascade_errors": cascade_count,
            "estimated_real_errors": len(errors) - cascade_count,
        }
