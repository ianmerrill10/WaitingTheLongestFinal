"""
Build Debug Agent - Main orchestrator and CLI entry point.

Pipeline: build -> parse -> categorize -> suggest -> (auto-fix) -> report

Usage:
    python scripts/build_debug/build_agent.py --docker
    python scripts/build_debug/build_agent.py --docker --auto-fix
    python scripts/build_debug/build_agent.py --docker --resume
    python scripts/build_debug/build_agent.py --local
    python scripts/build_debug/build_agent.py --dry-run
"""

import argparse
import sys
import time
from datetime import datetime
from pathlib import Path

# Add project root to path for imports
PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

from scripts.build_debug import config
from scripts.build_debug.models import BuildSession, BuildErrorSeverity
from scripts.build_debug.error_parser import CompilerOutputParser, reset_error_counter
from scripts.build_debug.error_categorizer import ErrorCategorizer
from scripts.build_debug.fix_suggester import FixSuggester
from scripts.build_debug.auto_fixer import AutoFixer
from scripts.build_debug.docker_builder import DockerBuilder
from scripts.build_debug.cmake_builder import CMakeBuilder
from scripts.build_debug.report_generator import BuildReportGenerator
from scripts.build_debug.session_tracker import SessionTracker


BANNER = r"""
 __        _______ _       ____        _ _     _    _                    _
 \ \      / /_   _| |     | __ ) _   _(_) | __| |  / \   __ _  ___ _ __ | |_
  \ \ /\ / /  | | | |     |  _ \| | | | | |/ _` | / _ \ / _` |/ _ \ '_ \| __|
   \ V  V /   | | | |___  | |_) | |_| | | | (_| |/ ___ \ (_| |  __/ | | | |_
    \_/\_/    |_| |_____| |____/ \__,_|_|_|\__,_/_/   \_\__, |\___|_| |_|\__|
                                                         |___/
  Build Debug Agent v1.0.0 - WaitingTheLongest.com
"""


class BuildDebugAgent:
    """Main orchestrator: build -> parse -> categorize -> suggest -> report."""

    def __init__(
        self,
        use_docker: bool = True,
        auto_fix: bool = False,
        dry_run: bool = False,
        max_attempts: int = 1,
        no_cache: bool = True,
        project_root: str = None,
    ):
        self.use_docker = use_docker
        self.auto_fix = auto_fix
        self.dry_run = dry_run
        self.max_attempts = max_attempts
        self.no_cache = no_cache
        self.project_root = Path(project_root) if project_root else config.PROJECT_ROOT

        # Initialize components
        self.parser = CompilerOutputParser()
        self.categorizer = ErrorCategorizer()
        self.suggester = FixSuggester()
        self.auto_fixer = AutoFixer(
            project_root=str(self.project_root), dry_run=dry_run
        )
        self.reporter = BuildReportGenerator()
        self.tracker = SessionTracker()

        # Builder
        if use_docker:
            self.builder = DockerBuilder(project_root=str(self.project_root))
        else:
            self.builder = CMakeBuilder(project_root=str(self.project_root))

    def run(self) -> BuildSession:
        """Execute the full pipeline: build, parse, categorize, report."""
        print(BANNER)
        config.ensure_dirs()

        # Pre-flight checks
        if not self._preflight_checks():
            sys.exit(1)

        # Start session
        session = self.tracker.start_session()

        # Run build attempts
        for attempt_num in range(1, self.max_attempts + 1):
            print(f"\n{'=' * 60}")
            print(f"  BUILD ATTEMPT #{attempt_num} of {self.max_attempts}")
            print(f"{'=' * 60}\n")

            # Execute build
            exit_code, raw_output, duration = self._execute_build()

            # Parse errors
            errors = self._parse_errors(raw_output)

            # Categorize
            errors = self._categorize_errors(errors)

            # Get cascade estimate
            cascade = self.categorizer.get_cascade_estimate(errors)
            print(f"\n[Analysis] Cascade estimate:")
            print(f"  Missing headers: {cascade['missing_header_count']}")
            print(f"  Files affected: {cascade['files_with_missing_headers']}")
            print(f"  Estimated cascade errors: {cascade['estimated_cascade_errors']}")
            print(f"  Estimated real errors: {cascade['estimated_real_errors']}")

            # Generate suggestions
            errors = self._suggest_fixes(errors)

            # Record attempt
            self.tracker.record_attempt(
                session=session,
                exit_code=exit_code,
                duration=duration,
                errors=errors,
                build_command="docker build" if self.use_docker else "cmake --build",
            )

            # Check if build succeeded
            if exit_code == 0:
                print("\n" + "=" * 60)
                print("  BUILD SUCCEEDED!")
                print("=" * 60)
                break

            # Auto-fix if enabled and not last attempt
            if self.auto_fix and attempt_num < self.max_attempts:
                print(f"\n[AutoFix] Applying safe fixes...")
                unfixed = session.unfixed_errors()
                fixes = self.auto_fixer.fix_all(unfixed, attempt_number=attempt_num)
                session.all_fixes.extend(fixes)

                applied = sum(1 for f in fixes if f.auto_applied)
                suggested = sum(1 for f in fixes if not f.auto_applied)
                print(f"[AutoFix] Applied: {applied}, Suggested only: {suggested}")

                if applied == 0:
                    print("[AutoFix] No auto-fixes available. Stopping.")
                    break

        # Generate reports
        print(f"\n[Reports] Generating reports...")
        files = self._generate_reports(session)

        # Save session
        self.tracker.save_session(session)

        # Final summary
        self._print_summary(session, files)

        return session

    def run_incremental(self) -> BuildSession:
        """Resume from previous session, rebuild, compare."""
        print(BANNER)
        config.ensure_dirs()

        # Load previous session
        session = self.tracker.resume_session()
        if not session:
            print("[Session] No previous session found. Starting fresh.")
            return self.run()

        print(f"[Session] Resuming session: {session.session_id}")
        print(f"  Previous attempts: {len(session.attempts)}")
        print(f"  Previous errors: {session.total_unique_errors}")
        print(f"  Previously fixed: {session.total_fixed}")

        # Pre-flight checks
        if not self._preflight_checks():
            sys.exit(1)

        # Run one more attempt
        print(f"\n{'=' * 60}")
        print(f"  BUILD ATTEMPT #{len(session.attempts) + 1} (resumed)")
        print(f"{'=' * 60}\n")

        exit_code, raw_output, duration = self._execute_build()
        errors = self._parse_errors(raw_output)
        errors = self._categorize_errors(errors)
        errors = self._suggest_fixes(errors)

        self.tracker.record_attempt(
            session=session,
            exit_code=exit_code,
            duration=duration,
            errors=errors,
            build_command="docker build" if self.use_docker else "cmake --build",
        )

        # Compare with previous
        if len(session.attempts) >= 2:
            comparison = self.tracker.compare_attempts(session)
            print(f"\n[Comparison] vs previous attempt:")
            print(f"  Error delta: {comparison['error_delta']:+d}")
            print(f"  New errors: {comparison['new_errors']}")
            print(f"  Fixed errors: {comparison['fixed_errors']}")
            print(f"  Trend: {comparison['overall_trend']}")

        # Generate reports
        files = self._generate_reports(session)
        self.tracker.save_session(session)
        self._print_summary(session, files)

        return session

    # =========================================================================
    # Pipeline steps
    # =========================================================================

    def _preflight_checks(self) -> bool:
        """Verify build environment is ready."""
        print("[Preflight] Checking build environment...")

        if self.use_docker:
            ok, msg = self.builder.check_docker_available()
            print(f"  Docker: {'OK' if ok else 'FAIL'} - {msg}")
            if not ok:
                print("\nDocker is required for --docker mode.")
                print("Install Docker or use --local for CMake builds.")
                return False
        else:
            ok, msg = self.builder.check_cmake_available()
            print(f"  CMake: {'OK' if ok else 'FAIL'} - {msg}")
            if not ok:
                return False

            ok, msg = self.builder.check_compiler_available()
            print(f"  Compiler: {'OK' if ok else 'FAIL'} - {msg}")
            if not ok:
                return False

        # Check project structure
        cmake_exists = config.CMAKE_LISTS_PATH.exists()
        src_exists = config.SRC_DIR.exists()
        print(f"  CMakeLists.txt: {'OK' if cmake_exists else 'MISSING'}")
        print(f"  src/ directory: {'OK' if src_exists else 'MISSING'}")

        if not cmake_exists or not src_exists:
            print("\nProject structure incomplete.")
            return False

        print("[Preflight] All checks passed.\n")
        return True

    def _execute_build(self) -> tuple[int, str, float]:
        """Run the build via Docker or CMake."""
        if self.use_docker:
            return self.builder.build(no_cache=self.no_cache)
        else:
            return self.builder.build()

    def _parse_errors(self, raw_output: str) -> list:
        """Parse raw build output into BuildError objects."""
        compiler = "gcc"  # Default for Docker/Ubuntu
        errors = self.parser.parse(raw_output, compiler=compiler)

        error_count = sum(
            1 for e in errors
            if e.severity in (BuildErrorSeverity.ERROR, BuildErrorSeverity.FATAL)
        )
        warning_count = sum(
            1 for e in errors if e.severity == BuildErrorSeverity.WARNING
        )

        print(f"[Parser] Parsed {len(errors)} unique items ({error_count} errors, {warning_count} warnings)")
        return errors

    def _categorize_errors(self, errors: list) -> list:
        """Classify errors into categories."""
        errors = self.categorizer.categorize_batch(errors)

        # Print category breakdown
        cats = {}
        for e in errors:
            cats[e.category.value] = cats.get(e.category.value, 0) + 1

        print(f"[Categorizer] Classification:")
        for cat, count in sorted(cats.items(), key=lambda x: x[1], reverse=True):
            print(f"  {cat}: {count}")

        return errors

    def _suggest_fixes(self, errors: list) -> list:
        """Generate fix suggestions."""
        errors = self.suggester.suggest_batch(errors)
        suggested = sum(1 for e in errors if e.suggestion)
        print(f"[Suggester] Generated {suggested} fix suggestions")
        return errors

    def _generate_reports(self, session: BuildSession) -> dict:
        """Generate all report files."""
        files = self.reporter.generate_all(session)

        print(f"\n[Reports] Generated files:")
        for name, filepath in files.items():
            size_kb = filepath.stat().st_size / 1024
            print(f"  {name}: {filepath.name} ({size_kb:.1f} KB)")

        return files

    def _print_summary(self, session: BuildSession, files: dict):
        """Print final session summary."""
        print(f"\n{'=' * 60}")
        print(f"  BUILD DEBUG SESSION SUMMARY")
        print(f"{'=' * 60}")
        print(f"  Session:    {session.session_id}")
        print(f"  Attempts:   {len(session.attempts)}")
        print(f"  Status:     {session.current_status}")
        print(f"  Errors:     {session.total_unique_errors}")
        print(f"  Fixed:      {session.total_fixed} ({session.fix_rate:.1%})")
        print(f"  Remaining:  {session.total_remaining}")
        print(f"  Fixes:      {len(session.all_fixes)} applied")
        print(f"{'=' * 60}")

        if session.total_remaining > 0:
            print(f"\n  Next steps:")
            print(f"  1. Review: build_logs/build_report_ai.txt (for AI analysis)")
            print(f"  2. Review: build_logs/build_report.txt (human-readable)")
            print(f"  3. Fix errors, then run: python {__file__} --resume")

        if session.current_status == "build_succeeded":
            print(f"\n  BUILD SUCCEEDED! The project compiles.")


def main():
    parser = argparse.ArgumentParser(
        description="WaitingTheLongest Build Debug Agent",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python scripts/build_debug/build_agent.py --docker
  python scripts/build_debug/build_agent.py --docker --auto-fix --max-attempts 5
  python scripts/build_debug/build_agent.py --docker --resume
  python scripts/build_debug/build_agent.py --local
  python scripts/build_debug/build_agent.py --dry-run
        """,
    )

    build_group = parser.add_mutually_exclusive_group()
    build_group.add_argument(
        "--docker",
        action="store_true",
        default=True,
        help="Build using Docker (default)",
    )
    build_group.add_argument(
        "--local", action="store_true", help="Build using local CMake"
    )

    parser.add_argument(
        "--auto-fix",
        action="store_true",
        help="Apply safe automated fixes between attempts",
    )
    parser.add_argument(
        "--max-attempts",
        type=int,
        default=1,
        help="Maximum build attempts (default: 1)",
    )
    parser.add_argument(
        "--resume",
        action="store_true",
        help="Resume from previous session",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Parse and report only, don't apply fixes",
    )
    parser.add_argument(
        "--no-cache",
        action="store_true",
        default=True,
        help="Docker: don't use build cache (default: true)",
    )
    parser.add_argument(
        "--use-cache",
        action="store_true",
        help="Docker: use build cache for faster rebuilds",
    )
    parser.add_argument(
        "--project-root",
        type=str,
        default=None,
        help="Override project root directory",
    )

    args = parser.parse_args()

    use_docker = not args.local
    no_cache = not args.use_cache

    agent = BuildDebugAgent(
        use_docker=use_docker,
        auto_fix=args.auto_fix,
        dry_run=args.dry_run,
        max_attempts=args.max_attempts,
        no_cache=no_cache,
        project_root=args.project_root,
    )

    if args.resume:
        session = agent.run_incremental()
    else:
        session = agent.run()

    # Exit with build's exit code
    if session.latest_attempt:
        sys.exit(0 if session.latest_attempt.exit_code == 0 else 1)
    sys.exit(1)


if __name__ == "__main__":
    main()
