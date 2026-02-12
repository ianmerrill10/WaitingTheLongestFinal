"""
Build Debug Agent - Safe automated fixer.

Applies only deterministic, additive, non-breaking fixes:
- Adding missing #include directives (when symbol->header mapping is known)
- Fixing namespace qualification
- Adding static_cast for narrowing conversions

Never auto-fixes: template errors, access violations, missing implementations.
"""

import re
from pathlib import Path
from typing import Optional
from datetime import datetime

from .models import BuildError, BuildErrorCategory, FixAttempt, FixStatus
from .fix_suggester import SYMBOL_TO_HEADER, NAMESPACE_FIXES
from . import config

# Counter for fix IDs
_fix_counter = 0


def _next_fix_id() -> str:
    global _fix_counter
    _fix_counter += 1
    return f"fix_{_fix_counter:04d}"


class AutoFixer:
    """Applies automated fixes for common, deterministic error patterns."""

    def __init__(self, project_root: str = None, dry_run: bool = False):
        self.project_root = Path(project_root) if project_root else config.PROJECT_ROOT
        self.dry_run = dry_run
        self._file_cache: dict[str, list[str]] = {}  # filepath -> lines

    def fix_all(
        self, errors: list[BuildError], attempt_number: int = 0
    ) -> list[FixAttempt]:
        """Attempt to fix all errors that have safe auto-fix patterns."""
        fixes = []

        # Sort errors by category priority (fix headers first for cascade effect)
        priority_order = [
            BuildErrorCategory.MISSING_HEADER,
            BuildErrorCategory.NAMESPACE_ERROR,
            BuildErrorCategory.IMPLICIT_CONVERSION,
            BuildErrorCategory.SYNTAX_ERROR,
        ]

        sorted_errors = sorted(
            errors,
            key=lambda e: (
                priority_order.index(e.category)
                if e.category in priority_order
                else len(priority_order)
            ),
        )

        for error in sorted_errors:
            fix = self._try_fix(error, attempt_number)
            if fix:
                fixes.append(fix)

        # Flush all modified files
        if not self.dry_run:
            self._flush_all()

        return fixes

    def _try_fix(
        self, error: BuildError, attempt_number: int
    ) -> Optional[FixAttempt]:
        """Try to fix a single error. Returns FixAttempt or None."""
        handlers = {
            BuildErrorCategory.MISSING_HEADER: self._fix_missing_include,
            BuildErrorCategory.NAMESPACE_ERROR: self._fix_namespace_error,
            BuildErrorCategory.IMPLICIT_CONVERSION: self._fix_narrowing_conversion,
            BuildErrorCategory.SYNTAX_ERROR: self._fix_missing_semicolon,
        }

        handler = handlers.get(error.category)
        if not handler:
            return None

        return handler(error, attempt_number)

    def _fix_missing_include(
        self, error: BuildError, attempt_number: int
    ) -> Optional[FixAttempt]:
        """Add missing #include directive."""
        # Extract the missing header from the error message
        header_match = re.search(
            r"['\"`]?([^\s'`\"]+\.h(?:pp)?)['\"`]?:\s*No such file or directory",
            error.message,
        )
        if not header_match:
            # Try alternate pattern
            header_match = re.search(
                r"['\"`]([^\s'`\"]+\.h(?:pp)?)['\"`]\s*file not found",
                error.message,
            )

        if not header_match:
            return None

        missing_header = header_match.group(1)

        # Resolve the file path
        filepath = self._resolve_path(error.file)
        if not filepath or not filepath.exists():
            return None

        # Read file
        lines = self._get_lines(str(filepath))
        if not lines:
            return None

        # Check if include already exists
        include_directive = f'#include "{missing_header}"'
        for line in lines:
            if missing_header in line and "#include" in line:
                return None  # Already included

        # Find the best insertion point (after last #include)
        insert_idx = 0
        for i, line in enumerate(lines):
            stripped = line.strip()
            if stripped.startswith("#include"):
                insert_idx = i + 1
            elif stripped and not stripped.startswith("//") and not stripped.startswith("/*") and not stripped.startswith("*") and insert_idx > 0:
                break

        # Insert the include
        if not self.dry_run:
            lines.insert(insert_idx, include_directive + "\n")
            self._set_lines(str(filepath), lines)

        return FixAttempt(
            id=_next_fix_id(),
            error_id=error.id,
            category=error.category,
            description=f"Added {include_directive}",
            file_modified=str(filepath),
            change_description=f"Inserted include at line {insert_idx + 1}",
            status=FixStatus.APPLIED if not self.dry_run else FixStatus.SUGGESTED,
            attempt_number=attempt_number,
            auto_applied=not self.dry_run,
        )

    def _fix_namespace_error(
        self, error: BuildError, attempt_number: int
    ) -> Optional[FixAttempt]:
        """Fix namespace qualification issues."""
        # Look for identifiers that we know the correct include for
        identifiers = re.findall(r"['\"`](\w+)['\"`]", error.message)

        for ident in identifiers:
            if ident in SYMBOL_TO_HEADER:
                header = SYMBOL_TO_HEADER[ident]

                filepath = self._resolve_path(error.file)
                if not filepath or not filepath.exists():
                    continue

                lines = self._get_lines(str(filepath))
                if not lines:
                    continue

                # Check if header is already included
                header_str = header.strip('"').strip("<").strip(">")
                already_included = any(header_str in line for line in lines)
                if already_included:
                    continue

                # Find insertion point
                insert_idx = 0
                for i, line in enumerate(lines):
                    if line.strip().startswith("#include"):
                        insert_idx = i + 1

                include_directive = f"#include {header}"
                if not self.dry_run:
                    lines.insert(insert_idx, include_directive + "\n")
                    self._set_lines(str(filepath), lines)

                return FixAttempt(
                    id=_next_fix_id(),
                    error_id=error.id,
                    category=error.category,
                    description=f"Added {include_directive} for '{ident}'",
                    file_modified=str(filepath),
                    change_description=f"Inserted include at line {insert_idx + 1}",
                    status=FixStatus.APPLIED if not self.dry_run else FixStatus.SUGGESTED,
                    attempt_number=attempt_number,
                    auto_applied=not self.dry_run,
                )

        return None

    def _fix_narrowing_conversion(
        self, error: BuildError, attempt_number: int
    ) -> Optional[FixAttempt]:
        """
        Add static_cast for narrowing conversions.
        Only suggests - doesn't auto-apply (too risky without full context).
        """
        conv_match = re.search(
            r"from ['\"`]([^'`\"]+)['\"`] to ['\"`]([^'`\"]+)['\"`]",
            error.message,
        )
        if not conv_match:
            return None

        from_type = conv_match.group(1)
        to_type = conv_match.group(2)

        return FixAttempt(
            id=_next_fix_id(),
            error_id=error.id,
            category=error.category,
            description=f"Wrap expression with static_cast<{to_type}>(...)",
            file_modified=error.file,
            change_description=f"At line {error.line}: static_cast<{to_type}>(expr) needed",
            status=FixStatus.SUGGESTED,  # Never auto-apply casts
            attempt_number=attempt_number,
            auto_applied=False,
        )

    def _fix_missing_semicolon(
        self, error: BuildError, attempt_number: int
    ) -> Optional[FixAttempt]:
        """Add missing semicolons (only for very clear cases)."""
        if "expected ';'" not in error.message:
            return None

        filepath = self._resolve_path(error.file)
        if not filepath or not filepath.exists():
            return None

        lines = self._get_lines(str(filepath))
        if not lines:
            return None

        line_idx = error.line - 1
        if line_idx < 0 or line_idx >= len(lines):
            return None

        line = lines[line_idx].rstrip("\n")

        # Only fix if the line ends with something that should have a semicolon
        # (class/struct definitions, return statements, etc.)
        safe_patterns = [
            re.compile(r"^\s*\}\s*$"),  # closing brace of class/struct
            re.compile(r"^\s*return\s+.+[^;]\s*$"),  # return without semicolon
        ]

        for pattern in safe_patterns:
            if pattern.match(line):
                if not self.dry_run:
                    lines[line_idx] = line + ";\n"
                    self._set_lines(str(filepath), lines)

                return FixAttempt(
                    id=_next_fix_id(),
                    error_id=error.id,
                    category=error.category,
                    description=f"Added missing semicolon",
                    file_modified=str(filepath),
                    change_description=f"Added ';' at end of line {error.line}",
                    status=FixStatus.APPLIED if not self.dry_run else FixStatus.SUGGESTED,
                    attempt_number=attempt_number,
                    auto_applied=not self.dry_run,
                )

        return None

    # =========================================================================
    # File I/O helpers
    # =========================================================================

    def _resolve_path(self, filepath: str) -> Optional[Path]:
        """Resolve a compiler-reported path to an actual file."""
        p = Path(filepath)
        if p.is_absolute() and p.exists():
            return p

        # Try relative to project root
        resolved = self.project_root / filepath
        if resolved.exists():
            return resolved

        # Try under src/
        resolved = self.project_root / "src" / filepath
        if resolved.exists():
            return resolved

        # Try stripping build path prefix
        for prefix in ["/build/app/", "/build/app/src/", "src/"]:
            if filepath.startswith(prefix):
                stripped = filepath[len(prefix):]
                resolved = self.project_root / "src" / stripped
                if resolved.exists():
                    return resolved

        return None

    def _get_lines(self, filepath: str) -> list[str]:
        """Read file lines with caching."""
        if filepath not in self._file_cache:
            try:
                with open(filepath, "r", encoding="utf-8") as f:
                    self._file_cache[filepath] = f.readlines()
            except (OSError, UnicodeDecodeError):
                return []
        return self._file_cache[filepath]

    def _set_lines(self, filepath: str, lines: list[str]):
        """Update cached lines (flushed later)."""
        self._file_cache[filepath] = lines

    def _flush_all(self):
        """Write all modified files to disk."""
        for filepath, lines in self._file_cache.items():
            try:
                with open(filepath, "w", encoding="utf-8") as f:
                    f.writelines(lines)
            except OSError as e:
                print(f"[AutoFixer] Failed to write {filepath}: {e}")

        self._file_cache.clear()
