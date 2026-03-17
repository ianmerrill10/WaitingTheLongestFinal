"""
Build Debug Agent - Session tracker.

Tracks multi-attempt build sessions with save/resume support.
Compares errors between attempts to detect progress.
"""

import json
from datetime import datetime
from pathlib import Path
from typing import Optional

from .models import (
    BuildSession,
    BuildAttempt,
    BuildError,
    BuildErrorCategory,
    BuildErrorSeverity,
)
from . import config


class SessionTracker:
    """Tracks incremental build attempts within a session."""

    def __init__(self, session_dir: str = None):
        self.session_dir = Path(session_dir) if session_dir else config.BUILD_LOGS_DIR

    def start_session(self) -> BuildSession:
        """Create a new build session."""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        session_id = f"build_{timestamp}"

        # Count source files
        source_count = 0
        src_dir = config.SRC_DIR
        if src_dir.exists():
            for ext in config.CPP_SOURCE_EXTENSIONS:
                source_count += len(list(src_dir.rglob(f"*{ext}")))

        session = BuildSession(
            session_id=session_id,
            start_time=datetime.now().isoformat(),
            total_source_files=source_count,
        )

        print(f"[Session] Started new session: {session_id}")
        print(f"[Session] Source files found: {source_count}")

        return session

    def record_attempt(
        self,
        session: BuildSession,
        exit_code: int,
        duration: float,
        errors: list[BuildError],
        build_command: str = "",
        raw_output_path: str = None,
    ) -> BuildAttempt:
        """Record a build attempt and update session state."""
        attempt_number = len(session.attempts) + 1

        # Separate errors and warnings
        error_count = sum(
            1 for e in errors
            if e.severity in (BuildErrorSeverity.ERROR, BuildErrorSeverity.FATAL)
        )
        warning_count = sum(
            1 for e in errors if e.severity == BuildErrorSeverity.WARNING
        )

        # Count by category
        by_cat = {}
        for error in errors:
            cat = error.category.value
            by_cat[cat] = by_cat.get(cat, 0) + 1

        # Compare with previous attempt
        prev_error_keys = set()
        if session.attempts:
            prev_error_keys = set(
                e.dedup_key()
                for e in session.all_errors.values()
                if e.fixed_in_attempt is None
            )

        current_error_keys = set(e.dedup_key() for e in errors)

        # Detect new and fixed errors
        new_keys = current_error_keys - prev_error_keys
        fixed_keys = prev_error_keys - current_error_keys

        # Mark fixed errors
        for error_id, error in session.all_errors.items():
            if error.dedup_key() in fixed_keys and error.fixed_in_attempt is None:
                error.fixed_in_attempt = attempt_number

        # Add new errors to session
        new_count = 0
        for error in errors:
            key = error.dedup_key()
            existing = None
            for e in session.all_errors.values():
                if e.dedup_key() == key:
                    existing = e
                    break

            if existing:
                existing.last_seen_attempt = attempt_number
                existing.occurrence_count += 1
                if key not in fixed_keys:
                    existing.fixed_in_attempt = None  # Still present
            else:
                error.first_seen_attempt = attempt_number
                error.last_seen_attempt = attempt_number
                session.all_errors[error.id] = error
                new_count += 1

        # Create attempt record
        attempt = BuildAttempt(
            attempt_number=attempt_number,
            timestamp=datetime.now().isoformat(),
            build_command=build_command,
            exit_code=exit_code,
            duration_seconds=round(duration, 1),
            total_errors=error_count,
            total_warnings=warning_count,
            errors_by_category=by_cat,
            new_errors=new_count,
            fixed_errors=len(fixed_keys),
            raw_output_path=raw_output_path,
        )

        session.attempts.append(attempt)

        # Update session status
        if exit_code == 0:
            session.current_status = "build_succeeded"
        else:
            session.current_status = "in_progress"

        # Print summary
        print(f"\n[Session] Attempt #{attempt_number} Results:")
        print(f"  Exit code:  {exit_code}")
        print(f"  Duration:   {duration:.1f}s")
        print(f"  Errors:     {error_count}")
        print(f"  Warnings:   {warning_count}")
        print(f"  New errors: {new_count}")
        print(f"  Fixed:      {len(fixed_keys)}")
        print(f"  Total unique errors (session): {session.total_unique_errors}")
        print(f"  Total remaining: {session.total_remaining}")

        return attempt

    def compare_attempts(
        self, session: BuildSession, prev_idx: int = -2, curr_idx: int = -1
    ) -> dict:
        """Compare two attempts to show progress."""
        if len(session.attempts) < 2:
            return {"message": "Need at least 2 attempts to compare"}

        prev = session.attempts[prev_idx]
        curr = session.attempts[curr_idx]

        # Category changes
        cat_changes = {}
        all_cats = set(prev.errors_by_category.keys()) | set(
            curr.errors_by_category.keys()
        )
        for cat in all_cats:
            prev_count = prev.errors_by_category.get(cat, 0)
            curr_count = curr.errors_by_category.get(cat, 0)
            delta = curr_count - prev_count
            if delta != 0:
                cat_changes[cat] = {
                    "previous": prev_count,
                    "current": curr_count,
                    "delta": delta,
                    "direction": "improved" if delta < 0 else "regressed",
                }

        return {
            "previous_attempt": prev.attempt_number,
            "current_attempt": curr.attempt_number,
            "error_delta": curr.total_errors - prev.total_errors,
            "warning_delta": curr.total_warnings - prev.total_warnings,
            "new_errors": curr.new_errors,
            "fixed_errors": curr.fixed_errors,
            "category_changes": cat_changes,
            "overall_trend": (
                "improving"
                if curr.total_errors < prev.total_errors
                else "regressing"
                if curr.total_errors > prev.total_errors
                else "stable"
            ),
        }

    def save_session(self, session: BuildSession):
        """Persist session state to disk."""
        self.session_dir.mkdir(parents=True, exist_ok=True)
        config.SESSION_FILE.write_text(session.to_json(), encoding="utf-8")
        print(f"[Session] Saved to {config.SESSION_FILE}")

    def load_session(self, session_id: str = None) -> Optional[BuildSession]:
        """Load a session from disk."""
        if not config.SESSION_FILE.exists():
            return None

        try:
            data = config.SESSION_FILE.read_text(encoding="utf-8")
            session = BuildSession.from_json(data)

            if session_id and session.session_id != session_id:
                return None

            print(f"[Session] Loaded session: {session.session_id}")
            print(f"  Attempts: {len(session.attempts)}")
            print(f"  Errors tracked: {session.total_unique_errors}")
            print(f"  Fixed: {session.total_fixed}")

            return session
        except (json.JSONDecodeError, KeyError, TypeError) as e:
            print(f"[Session] Failed to load session: {e}")
            return None

    def resume_session(self) -> Optional[BuildSession]:
        """Load the most recent incomplete session."""
        session = self.load_session()
        if session and session.current_status != "build_succeeded":
            return session
        return None
