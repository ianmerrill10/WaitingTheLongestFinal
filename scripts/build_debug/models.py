"""
Build Debug Agent - Data models.

Mirrors the runtime ErrorContext/ErrorCategory/ErrorSeverity from
src/core/debug/ErrorCapture.h but adapted for build-time errors.
"""

from dataclasses import dataclass, field, asdict
from enum import Enum
from typing import Optional
from datetime import datetime
import json


class BuildErrorCategory(Enum):
    MISSING_HEADER = "MISSING_HEADER"
    UNDEFINED_REFERENCE = "UNDEFINED_REFERENCE"
    TYPE_MISMATCH = "TYPE_MISMATCH"
    NAMESPACE_ERROR = "NAMESPACE_ERROR"
    SYNTAX_ERROR = "SYNTAX_ERROR"
    MISSING_METHOD = "MISSING_METHOD"
    DUPLICATE_SYMBOL = "DUPLICATE_SYMBOL"
    TEMPLATE_ERROR = "TEMPLATE_ERROR"
    ACCESS_ERROR = "ACCESS_ERROR"
    IMPLICIT_CONVERSION = "IMPLICIT_CONVERSION"
    CMAKE_ERROR = "CMAKE_ERROR"
    LINKER_ERROR = "LINKER_ERROR"
    UNKNOWN = "UNKNOWN"


class BuildErrorSeverity(Enum):
    NOTE = "note"
    WARNING = "warning"
    ERROR = "error"
    FATAL = "fatal error"


class FixStatus(Enum):
    SUGGESTED = "suggested"
    APPLIED = "applied"
    VERIFIED = "verified"
    FAILED = "failed"
    SKIPPED = "skipped"


@dataclass
class BuildError:
    id: str
    file: str
    line: int
    column: int
    severity: BuildErrorSeverity
    category: BuildErrorCategory
    message: str
    raw_output: str
    compiler: str = "gcc"
    suggestion: Optional[str] = None
    related_errors: list = field(default_factory=list)
    occurrence_count: int = 1
    first_seen_attempt: int = 0
    last_seen_attempt: int = 0
    fixed_in_attempt: Optional[int] = None
    metadata: dict = field(default_factory=dict)

    def to_dict(self) -> dict:
        d = asdict(self)
        d["severity"] = self.severity.value
        d["category"] = self.category.value
        return d

    @classmethod
    def from_dict(cls, d: dict) -> "BuildError":
        d["severity"] = BuildErrorSeverity(d["severity"])
        d["category"] = BuildErrorCategory(d["category"])
        return cls(**d)

    def dedup_key(self) -> str:
        return f"{self.file}:{self.line}:{self.column}:{self.message}"


@dataclass
class FixAttempt:
    id: str
    error_id: str
    category: BuildErrorCategory
    description: str
    file_modified: str
    change_description: str
    status: FixStatus
    attempt_number: int
    timestamp: str = ""
    auto_applied: bool = False

    def __post_init__(self):
        if not self.timestamp:
            self.timestamp = datetime.now().isoformat()

    def to_dict(self) -> dict:
        d = asdict(self)
        d["category"] = self.category.value
        d["status"] = self.status.value
        return d

    @classmethod
    def from_dict(cls, d: dict) -> "FixAttempt":
        d["category"] = BuildErrorCategory(d["category"])
        d["status"] = FixStatus(d["status"])
        return cls(**d)


@dataclass
class BuildAttempt:
    attempt_number: int
    timestamp: str
    build_command: str
    exit_code: int
    duration_seconds: float
    total_errors: int
    total_warnings: int
    errors_by_category: dict = field(default_factory=dict)
    new_errors: int = 0
    fixed_errors: int = 0
    raw_output_path: Optional[str] = None

    def to_dict(self) -> dict:
        return asdict(self)

    @classmethod
    def from_dict(cls, d: dict) -> "BuildAttempt":
        return cls(**d)


@dataclass
class BuildSession:
    session_id: str
    start_time: str
    project_name: str = "WaitingTheLongest"
    total_source_files: int = 0
    attempts: list = field(default_factory=list)
    all_errors: dict = field(default_factory=dict)  # id -> BuildError
    all_fixes: list = field(default_factory=list)
    current_status: str = "in_progress"

    def to_dict(self) -> dict:
        return {
            "session_id": self.session_id,
            "start_time": self.start_time,
            "project_name": self.project_name,
            "total_source_files": self.total_source_files,
            "attempts": [a.to_dict() for a in self.attempts],
            "all_errors": {k: v.to_dict() for k, v in self.all_errors.items()},
            "all_fixes": [f.to_dict() for f in self.all_fixes],
            "current_status": self.current_status,
        }

    @classmethod
    def from_dict(cls, d: dict) -> "BuildSession":
        session = cls(
            session_id=d["session_id"],
            start_time=d["start_time"],
            project_name=d.get("project_name", "WaitingTheLongest"),
            total_source_files=d.get("total_source_files", 0),
            current_status=d.get("current_status", "in_progress"),
        )
        session.attempts = [BuildAttempt.from_dict(a) for a in d.get("attempts", [])]
        session.all_errors = {
            k: BuildError.from_dict(v) for k, v in d.get("all_errors", {}).items()
        }
        session.all_fixes = [FixAttempt.from_dict(f) for f in d.get("all_fixes", [])]
        return session

    def to_json(self, indent: int = 2) -> str:
        return json.dumps(self.to_dict(), indent=indent, default=str)

    @classmethod
    def from_json(cls, json_str: str) -> "BuildSession":
        return cls.from_dict(json.loads(json_str))

    @property
    def latest_attempt(self) -> Optional[BuildAttempt]:
        return self.attempts[-1] if self.attempts else None

    @property
    def total_unique_errors(self) -> int:
        return len(self.all_errors)

    @property
    def total_fixed(self) -> int:
        return sum(1 for e in self.all_errors.values() if e.fixed_in_attempt is not None)

    @property
    def total_remaining(self) -> int:
        return self.total_unique_errors - self.total_fixed

    @property
    def fix_rate(self) -> float:
        if self.total_unique_errors == 0:
            return 0.0
        return self.total_fixed / self.total_unique_errors

    def errors_by_category(self) -> dict:
        counts = {}
        for error in self.all_errors.values():
            cat = error.category.value
            counts[cat] = counts.get(cat, 0) + 1
        return dict(sorted(counts.items(), key=lambda x: x[1], reverse=True))

    def errors_by_severity(self) -> dict:
        counts = {}
        for error in self.all_errors.values():
            sev = error.severity.value
            counts[sev] = counts.get(sev, 0) + 1
        return dict(sorted(counts.items(), key=lambda x: x[1], reverse=True))

    def unfixed_errors(self) -> list:
        return [e for e in self.all_errors.values() if e.fixed_in_attempt is None]

    def errors_for_file(self, filepath: str) -> list:
        return [e for e in self.all_errors.values() if e.file == filepath]
