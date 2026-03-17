"""
Build Debug Agent - Report generator.

Generates 5 output files mirroring the runtime LogGenerator formats:
1. build_errors.json  - Structured machine-parseable
2. build_report.txt   - Human-readable with stats
3. build_report_ai.txt - AI-optimized with analysis prompts
4. build_fixes.json   - Fix attempt tracking
5. BUILD_SESSION.md   - Narrative session log
"""

import json
from datetime import datetime
from pathlib import Path

from .models import BuildSession, BuildError, BuildErrorCategory, BuildErrorSeverity
from . import config


class BuildReportGenerator:
    """Generates all 5 output files."""

    def __init__(self, output_dir: str = None):
        self.output_dir = Path(output_dir) if output_dir else config.BUILD_LOGS_DIR

    def generate_all(self, session: BuildSession) -> dict[str, Path]:
        """Generate all 5 report files. Returns dict of name -> filepath."""
        self.output_dir.mkdir(parents=True, exist_ok=True)

        files = {}
        files["errors_json"] = self._write(
            config.ERRORS_JSON, self.generate_json_report(session)
        )
        files["report_txt"] = self._write(
            config.REPORT_TXT, self.generate_text_report(session)
        )
        files["report_ai"] = self._write(
            config.REPORT_AI_TXT, self.generate_ai_report(session)
        )
        files["fixes_json"] = self._write(
            config.FIXES_JSON, self.generate_fixes_json(session)
        )
        files["session_md"] = self._write(
            config.SESSION_MD, self.generate_session_md(session)
        )

        return files

    def _write(self, filepath: Path, content: str) -> Path:
        filepath.write_text(content, encoding="utf-8")
        return filepath

    # =========================================================================
    # 1. build_errors.json
    # =========================================================================

    def generate_json_report(self, session: BuildSession) -> str:
        """Machine-parseable structured error report."""
        report = {
            "session_id": session.session_id,
            "generated_at": datetime.now().isoformat(),
            "project": session.project_name,
            "build_environment": {
                "platform": "Docker (Ubuntu 22.04)" if session.attempts else "unknown",
                "compiler": "g++",
                "cpp_standard": "C++17",
            },
            "summary": {
                "total_attempts": len(session.attempts),
                "total_unique_errors": session.total_unique_errors,
                "total_fixed": session.total_fixed,
                "total_remaining": session.total_remaining,
                "fix_rate": round(session.fix_rate, 3),
                "by_category": session.errors_by_category(),
                "by_severity": session.errors_by_severity(),
            },
            "errors": [e.to_dict() for e in session.all_errors.values()],
            "attempts": [a.to_dict() for a in session.attempts],
        }

        return json.dumps(report, indent=2, default=str)

    # =========================================================================
    # 2. build_report.txt
    # =========================================================================

    def generate_text_report(self, session: BuildSession) -> str:
        """Human-readable report with statistics and grouped errors."""
        sep = "=" * 80
        lines = []

        # Header
        lines.append(sep)
        lines.append("                    WAITINGTHELONGEST BUILD REPORT")
        lines.append(sep)
        lines.append("")
        lines.append(f"Report Time:     {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        lines.append(f"Session ID:      {session.session_id}")
        lines.append(f"Build Attempts:  {len(session.attempts)}")
        lines.append(f"Source Files:    {session.total_source_files}")

        if session.latest_attempt:
            la = session.latest_attempt
            lines.append(f"Last Attempt:    #{la.attempt_number} (exit code {la.exit_code})")
            lines.append(f"Last Duration:   {la.duration_seconds:.1f}s")

        lines.append("")

        # Statistics
        lines.append(sep)
        lines.append("                           STATISTICS")
        lines.append(sep)
        lines.append("")
        lines.append(f"Total Unique Errors:    {session.total_unique_errors}")
        lines.append(f"Errors Fixed So Far:    {session.total_fixed}   ({session.fix_rate:.1%})")
        lines.append(f"Remaining Errors:       {session.total_remaining}")
        lines.append("")

        # By category
        lines.append("By Category:")
        by_cat = session.errors_by_category()
        if by_cat:
            max_name_len = max(len(k) for k in by_cat)
            for cat, count in by_cat.items():
                pct = count / session.total_unique_errors * 100 if session.total_unique_errors else 0
                lines.append(f"  {cat:<{max_name_len}}  {count:>5}   ({pct:.1f}%)")
        lines.append("")

        # By severity
        lines.append("By Severity:")
        by_sev = session.errors_by_severity()
        for sev, count in by_sev.items():
            lines.append(f"  {sev:<15}  {count:>5}")
        lines.append("")

        # Top priority errors (grouped by category)
        lines.append(sep)
        lines.append("                      TOP PRIORITY ERRORS")
        lines.append(sep)
        lines.append("")

        unfixed = session.unfixed_errors()
        by_cat_errors: dict[str, list[BuildError]] = {}
        for error in unfixed:
            by_cat_errors.setdefault(error.category.value, []).append(error)

        priority_order = [
            BuildErrorCategory.MISSING_HEADER,
            BuildErrorCategory.NAMESPACE_ERROR,
            BuildErrorCategory.UNDEFINED_REFERENCE,
            BuildErrorCategory.TYPE_MISMATCH,
            BuildErrorCategory.SYNTAX_ERROR,
            BuildErrorCategory.MISSING_METHOD,
            BuildErrorCategory.TEMPLATE_ERROR,
            BuildErrorCategory.DUPLICATE_SYMBOL,
            BuildErrorCategory.ACCESS_ERROR,
            BuildErrorCategory.IMPLICIT_CONVERSION,
            BuildErrorCategory.CMAKE_ERROR,
            BuildErrorCategory.LINKER_ERROR,
            BuildErrorCategory.UNKNOWN,
        ]

        for cat in priority_order:
            cat_name = cat.value
            cat_errors = by_cat_errors.get(cat_name, [])
            if not cat_errors:
                continue

            lines.append(f"--- {cat_name} ({len(cat_errors)} errors) ---")
            lines.append("")

            # Show top 10 unique files
            files_affected = set(e.file for e in cat_errors)
            lines.append(f"  Files affected: {len(files_affected)}")

            for error in cat_errors[:10]:
                lines.append(f"  [{error.id}] {error.file}:{error.line}: {error.message[:100]}")
                if error.suggestion:
                    first_line = error.suggestion.split("\n")[0]
                    lines.append(f"         FIX: {first_line}")

            if len(cat_errors) > 10:
                lines.append(f"  ... and {len(cat_errors) - 10} more")
            lines.append("")

        # Error details (full listing)
        lines.append(sep)
        lines.append("                        ALL ERRORS BY FILE")
        lines.append(sep)
        lines.append("")

        by_file: dict[str, list[BuildError]] = {}
        for error in unfixed:
            by_file.setdefault(error.file, []).append(error)

        for filepath in sorted(by_file.keys()):
            file_errors = sorted(by_file[filepath], key=lambda e: e.line)
            lines.append(f"--- {filepath} ({len(file_errors)} errors) ---")
            for error in file_errors:
                sev = error.severity.value.upper()
                lines.append(f"  L{error.line}:{error.column} [{sev}] {error.message[:120]}")
            lines.append("")

        lines.append(sep)
        lines.append("                        END OF REPORT")
        lines.append(sep)

        return "\n".join(lines)

    # =========================================================================
    # 3. build_report_ai.txt
    # =========================================================================

    def generate_ai_report(self, session: BuildSession) -> str:
        """AI-optimized report for Claude analysis."""
        lines = []

        lines.append("# WaitingTheLongest Build Error Analysis Report")
        lines.append("")
        lines.append("## Context for AI Analysis")
        lines.append("")
        lines.append(
            "This report contains build errors from WaitingTheLongest.com, a C++17 Drogon web "
            "application for dog rescue. The project has 319 source files (.cc) written by 34 "
            "AI agents and has NEVER been compiled. Your task is to analyze these errors, "
            "identify patterns, group related issues, and provide a prioritized fix plan."
        )
        lines.append("")
        lines.append("**Project Conventions:**")
        lines.append("- Namespace root: `wtl::`")
        lines.append("- Sub-namespaces: `wtl::core::services`, `wtl::modules`, `wtl::content::tiktok`, etc.")
        lines.append("- Singletons: `ClassName::getInstance()` pattern")
        lines.append("- Include paths relative to `src/`: e.g., `core/services/DogService.h`")
        lines.append("- Framework: Drogon HTTP + libpqxx + jsoncpp + OpenSSL")
        lines.append("")
        lines.append("**Build Environment:** Docker, Ubuntu 22.04, g++, CMake, C++17")
        lines.append("")

        # Executive summary
        lines.append("## Executive Summary")
        lines.append("")
        lines.append(f"- **Total Unique Errors:** {session.total_unique_errors}")
        lines.append(f"- **Total Warnings:** {sum(1 for e in session.all_errors.values() if e.severity == BuildErrorSeverity.WARNING)}")
        lines.append(f"- **Build Attempts:** {len(session.attempts)}")
        lines.append(f"- **Errors Fixed:** {session.total_fixed} ({session.fix_rate:.1%})")
        lines.append(f"- **Errors Remaining:** {session.total_remaining}")
        lines.append("")

        by_cat = session.errors_by_category()
        if by_cat:
            lines.append("**Error Distribution:**")
            lines.append("")
            lines.append("| Category | Count | % |")
            lines.append("|----------|-------|---|")
            for cat, count in by_cat.items():
                pct = count / session.total_unique_errors * 100 if session.total_unique_errors else 0
                lines.append(f"| {cat} | {count} | {pct:.1f}% |")
            lines.append("")

        # Cascade analysis
        unfixed = session.unfixed_errors()
        missing_header_files = set(
            e.file for e in unfixed if e.category == BuildErrorCategory.MISSING_HEADER
        )
        cascade_count = sum(
            1
            for e in unfixed
            if e.file in missing_header_files
            and e.category != BuildErrorCategory.MISSING_HEADER
        )

        if missing_header_files:
            mh_count = sum(1 for e in unfixed if e.category == BuildErrorCategory.MISSING_HEADER)
            lines.append("## Cascade Analysis")
            lines.append("")
            lines.append(
                f"**{mh_count} MISSING_HEADER errors** affect **{len(missing_header_files)} files**. "
                f"These prevent compilation of entire translation units, generating "
                f"**~{cascade_count} cascade errors** that will auto-resolve once headers are fixed."
            )
            lines.append("")
            lines.append(
                f"**Estimated real errors after fixing headers:** "
                f"~{session.total_remaining - cascade_count}"
            )
            lines.append("")

        # Errors by category with analysis guidance
        lines.append("## Errors by Category (Prioritized by Fix Impact)")
        lines.append("")

        analysis_prompts = {
            BuildErrorCategory.MISSING_HEADER: (
                "These cause cascade failures. Each missing header prevents compilation "
                "of the entire translation unit. FIX THESE FIRST."
            ),
            BuildErrorCategory.NAMESPACE_ERROR: (
                "Likely caused by 34 different agents using slightly different namespace "
                "conventions. Look for patterns like 'core::' instead of 'wtl::core::'."
            ),
            BuildErrorCategory.UNDEFINED_REFERENCE: (
                "These are linker errors. The symbol is declared but not defined. "
                "Check if .cc files are missing from CMakeLists.txt."
            ),
            BuildErrorCategory.TYPE_MISMATCH: (
                "Function signatures in headers don't match call sites. "
                "Agent consistency issue - verify .h matches .cc usage."
            ),
            BuildErrorCategory.SYNTAX_ERROR: (
                "Usually missing semicolons, unmatched braces, or typos. "
                "Often easy to fix once you see the context."
            ),
            BuildErrorCategory.MISSING_METHOD: (
                "Methods called that don't exist in the class. "
                "Either the method was never declared or was named differently."
            ),
            BuildErrorCategory.TEMPLATE_ERROR: (
                "Template instantiation failures. These are the hardest to fix. "
                "Review the full template context chain."
            ),
            BuildErrorCategory.DUPLICATE_SYMBOL: (
                "Usually functions defined in headers without 'inline', "
                "or the same source file included twice in CMakeLists.txt."
            ),
        }

        priority_cats = [
            BuildErrorCategory.MISSING_HEADER,
            BuildErrorCategory.NAMESPACE_ERROR,
            BuildErrorCategory.UNDEFINED_REFERENCE,
            BuildErrorCategory.TYPE_MISMATCH,
            BuildErrorCategory.SYNTAX_ERROR,
            BuildErrorCategory.MISSING_METHOD,
            BuildErrorCategory.TEMPLATE_ERROR,
            BuildErrorCategory.DUPLICATE_SYMBOL,
            BuildErrorCategory.ACCESS_ERROR,
            BuildErrorCategory.IMPLICIT_CONVERSION,
            BuildErrorCategory.CMAKE_ERROR,
            BuildErrorCategory.LINKER_ERROR,
            BuildErrorCategory.UNKNOWN,
        ]

        by_cat_errors: dict[str, list[BuildError]] = {}
        for error in unfixed:
            by_cat_errors.setdefault(error.category.value, []).append(error)

        for i, cat in enumerate(priority_cats, 1):
            cat_errors = by_cat_errors.get(cat.value, [])
            if not cat_errors:
                continue

            lines.append(f"### {i}. {cat.value} ({len(cat_errors)} errors)")
            lines.append("")

            prompt = analysis_prompts.get(cat, "Review these errors manually.")
            lines.append(f"*{prompt}*")
            lines.append("")

            # Show top 15 errors
            for error in cat_errors[:15]:
                lines.append(f"- `{error.file}:{error.line}`: {error.message[:150]}")
                if error.suggestion:
                    first_line = error.suggestion.split("\n")[0]
                    lines.append(f"  - **Fix:** {first_line}")

            if len(cat_errors) > 15:
                lines.append(f"- *... and {len(cat_errors) - 15} more*")
            lines.append("")

        # Pattern analysis
        lines.append("## Pattern Analysis")
        lines.append("")

        # Find most-errored files
        file_error_counts: dict[str, int] = {}
        for error in unfixed:
            file_error_counts[error.file] = file_error_counts.get(error.file, 0) + 1

        top_files = sorted(file_error_counts.items(), key=lambda x: x[1], reverse=True)[:15]
        if top_files:
            lines.append("**Most Problematic Files:**")
            lines.append("")
            lines.append("| File | Error Count |")
            lines.append("|------|-------------|")
            for filepath, count in top_files:
                lines.append(f"| `{filepath}` | {count} |")
            lines.append("")

        # Repeated messages
        msg_counts: dict[str, int] = {}
        for error in unfixed:
            # Normalize: strip specific file paths from message
            normalized = re.sub(r"['\"`][^'`\"]+['\"`]", "'...'", error.message)
            msg_counts[normalized] = msg_counts.get(normalized, 0) + 1

        repeated = [(msg, count) for msg, count in msg_counts.items() if count >= 3]
        repeated.sort(key=lambda x: x[1], reverse=True)

        if repeated:
            lines.append("**Repeated Error Patterns (3+ occurrences):**")
            lines.append("")
            for msg, count in repeated[:10]:
                lines.append(f"- ({count}x) `{msg[:120]}`")
            lines.append("")

        # Analysis questions
        lines.append("## Recommended Fix Order")
        lines.append("")
        lines.append("1. Fix MISSING_HEADER errors first (highest cascade impact)")
        lines.append("2. Fix NAMESPACE_ERROR (likely systematic convention mismatches)")
        lines.append("3. Add missing source files to CMakeLists.txt (UNDEFINED_REFERENCE)")
        lines.append("4. Fix TYPE_MISMATCH (align function signatures)")
        lines.append("5. Fix remaining categories")
        lines.append("")

        lines.append("## Analysis Questions")
        lines.append("")
        lines.append("1. Which errors cascade from MISSING_HEADER failures?")
        lines.append("2. Are there files with 5+ errors suggesting systemic issues?")
        lines.append("3. Which fixes would unblock the most compilation units?")
        lines.append("4. Are there patterns suggesting entire subsystems need rework?")
        lines.append("5. Which agent conventions conflict with each other?")
        lines.append("6. Are any .cc files missing from CMakeLists.txt?")
        lines.append("7. Are any header include paths wrong (relative to src/)?")

        return "\n".join(lines)

    # =========================================================================
    # 4. build_fixes.json
    # =========================================================================

    def generate_fixes_json(self, session: BuildSession) -> str:
        """Fix attempt tracking report."""
        total = len(session.all_fixes)
        succeeded = sum(1 for f in session.all_fixes if f.status in (FixStatus.APPLIED, FixStatus.VERIFIED))
        failed = sum(1 for f in session.all_fixes if f.status == FixStatus.FAILED)

        from .models import FixStatus

        report = {
            "session_id": session.session_id,
            "generated_at": datetime.now().isoformat(),
            "total_fixes_attempted": total,
            "total_fixes_succeeded": succeeded,
            "total_fixes_failed": failed,
            "fix_rate": round(succeeded / total, 3) if total > 0 else 0.0,
            "fixes": [f.to_dict() for f in session.all_fixes],
        }

        return json.dumps(report, indent=2, default=str)

    # =========================================================================
    # 5. BUILD_SESSION.md
    # =========================================================================

    def generate_session_md(self, session: BuildSession) -> str:
        """Human-readable session narrative log."""
        lines = []

        lines.append(f"# Build Session: {session.session_id}")
        lines.append("")
        lines.append("## Session Info")
        lines.append(f"- **Started:** {session.start_time}")
        lines.append(f"- **Project:** {session.project_name} v1.0.0")
        lines.append(f"- **Source Files:** {session.total_source_files}")
        lines.append(f"- **Status:** {session.current_status}")
        lines.append("")
        lines.append("---")

        for attempt in session.attempts:
            lines.append("")
            lines.append(f"## Attempt #{attempt.attempt_number} -- {attempt.timestamp}")
            lines.append(f"- **Duration:** {attempt.duration_seconds:.1f} seconds")
            lines.append(f"- **Exit Code:** {attempt.exit_code}")
            lines.append(f"- **Errors:** {attempt.total_errors} | **Warnings:** {attempt.total_warnings}")
            lines.append(f"- **New Errors:** {attempt.new_errors} | **Fixed Since Last:** {attempt.fixed_errors}")
            lines.append("")

            if attempt.errors_by_category:
                lines.append("### Error Breakdown")
                lines.append("")
                lines.append("| Category | Count |")
                lines.append("|----------|-------|")
                for cat, count in sorted(
                    attempt.errors_by_category.items(),
                    key=lambda x: x[1],
                    reverse=True,
                ):
                    lines.append(f"| {cat} | {count} |")
                lines.append("")

            lines.append("---")

        # Summary
        lines.append("")
        lines.append("## Session Summary")
        lines.append(f"- **Total Attempts:** {len(session.attempts)}")
        lines.append(f"- **Total Unique Errors Found:** {session.total_unique_errors}")
        lines.append(f"- **Errors Fixed:** {session.total_fixed}")
        lines.append(f"- **Errors Remaining:** {session.total_remaining}")
        lines.append(f"- **Overall Fix Rate:** {session.fix_rate:.1%}")
        lines.append(f"- **Total Fixes Applied:** {len(session.all_fixes)}")

        return "\n".join(lines)


# Need this import for the fixes_json method
import re
