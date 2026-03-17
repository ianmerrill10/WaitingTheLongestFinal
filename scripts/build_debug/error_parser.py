"""
Build Debug Agent - Compiler/Linker output parser.

Parses raw GCC, Clang, MSVC, and linker output into structured BuildError objects.
Handles Docker progress prefix stripping and multi-line template errors.
"""

import re
from typing import Optional
from .models import BuildError, BuildErrorSeverity, BuildErrorCategory

# Counter for unique error IDs
_error_counter = 0


def _next_error_id() -> str:
    global _error_counter
    _error_counter += 1
    return f"build_err_{_error_counter:04d}"


def reset_error_counter():
    global _error_counter
    _error_counter = 0


# ==============================================================================
# REGEX PATTERNS
# ==============================================================================

# Docker build output prefix: #15 0.123 or #15 [builder 12/12] RUN ...
DOCKER_PREFIX_RE = re.compile(
    r"^#\d+\s+(?:\d+\.\d+\s+)?(?:\[\w+[\s\w]* \d+/\d+\]\s+(?:RUN|COPY|WORKDIR|FROM)\s+)?"
)

# GCC/Clang: /path/to/file.cc:42:15: error: some message
GCC_ERROR_RE = re.compile(
    r"^(?P<file>[^\s:]+\.(?:cc|h|cpp|hpp|c|cxx)):(?P<line>\d+):(?P<col>\d+):\s+"
    r"(?P<severity>fatal error|error|warning|note):\s+(?P<message>.+)$"
)

# GCC/Clang alternate (no column): /path/to/file.cc:42: error: message
GCC_ERROR_NO_COL_RE = re.compile(
    r"^(?P<file>[^\s:]+\.(?:cc|h|cpp|hpp|c|cxx)):(?P<line>\d+):\s+"
    r"(?P<severity>fatal error|error|warning|note):\s+(?P<message>.+)$"
)

# In file included from file.cc:42:
INCLUDED_FROM_RE = re.compile(
    r"^In file included from (?P<file>[^\s:]+):(?P<line>\d+)"
)

# Linker: undefined reference to `symbol'
LINKER_UNDEF_RE = re.compile(
    r"undefined reference to [`'\"](?P<symbol>[^'`\"]+)[`'\"]"
)

# Linker: multiple definition of `symbol'
LINKER_MULTIDEF_RE = re.compile(
    r"multiple definition of [`'\"](?P<symbol>[^'`\"]+)[`'\"]"
)

# Linker: cannot find -lfoo
LINKER_MISSING_LIB_RE = re.compile(
    r"cannot find -l(?P<library>\S+)"
)

# Linker context: /path/to/file.o: in function `name':
LINKER_CONTEXT_RE = re.compile(
    r"(?P<file>[^\s:]+\.o):\s+in function [`'\"](?P<function>[^'`\"]+)[`'\"]"
)

# Linker: file.cc:(.text+0x42): message
LINKER_LOCATION_RE = re.compile(
    r"(?P<file>[^\s:]+\.(?:cc|cpp|c)):(?:\(.+\)):\s+(?P<message>.+)"
)

# CMake: CMake Error at CMakeLists.txt:42 (find_package):
CMAKE_ERROR_RE = re.compile(
    r"^CMake\s+(?P<severity>Error|Warning)"
    r"(?:\s+at\s+(?P<file>[^\s:]+):(?P<line>\d+))?"
    r"(?:\s+\((?P<command>\w+)\))?:\s*(?P<message>.+)$",
    re.MULTILINE,
)

# make[N]: *** [target] Error N
MAKE_ERROR_RE = re.compile(
    r"^make(?:\[\d+\])?:\s+\*\*\*\s+\[(?P<target>[^\]]+)\]\s+Error\s+(?P<code>\d+)"
)

# Template instantiation context
TEMPLATE_INST_RE = re.compile(
    r"(?:required from|in instantiation of|instantiated from)\s+"
)

# MSVC: file.cc(42,15): error C2065: message
MSVC_ERROR_RE = re.compile(
    r"^(?P<file>[^\s(]+\.(?:cc|h|cpp|hpp|c))\((?P<line>\d+)(?:,(?P<col>\d+))?\):\s+"
    r"(?P<severity>fatal error|error|warning|note)\s+(?P<code>C\d+):\s+(?P<message>.+)$"
)


class CompilerOutputParser:
    """Parses raw compiler/linker output into structured BuildError objects."""

    def __init__(self):
        self._include_stack = []  # Track "In file included from" chain

    def parse(self, raw_output: str, compiler: str = "gcc") -> list[BuildError]:
        """Parse raw build output into BuildError list."""
        # Strip Docker progress prefixes
        cleaned = self._strip_docker_prefixes(raw_output)

        errors = []
        lines = cleaned.split("\n")

        i = 0
        while i < len(lines):
            line = lines[i].strip()

            if not line:
                i += 1
                continue

            # Track include chains
            inc_match = INCLUDED_FROM_RE.match(line)
            if inc_match:
                self._include_stack.append(
                    (inc_match.group("file"), int(inc_match.group("line")))
                )
                i += 1
                continue

            # Try GCC/Clang format
            error = self._parse_gcc_line(line, compiler)
            if error:
                # Capture multi-line template context
                if TEMPLATE_INST_RE.search(line):
                    context_lines = [line]
                    j = i + 1
                    while j < len(lines) and j < i + 20:
                        next_line = lines[j].strip()
                        if not next_line or GCC_ERROR_RE.match(next_line):
                            break
                        if TEMPLATE_INST_RE.search(next_line) or "required from here" in next_line:
                            context_lines.append(next_line)
                        j += 1
                    if len(context_lines) > 1:
                        error.metadata["template_context"] = "\n".join(context_lines)

                # Attach include chain
                if self._include_stack:
                    error.metadata["included_from"] = [
                        f"{f}:{l}" for f, l in self._include_stack
                    ]
                    self._include_stack.clear()

                errors.append(error)
                i += 1
                continue

            # Try linker errors
            linker_errors = self._parse_linker_line(line, lines, i, compiler)
            if linker_errors:
                errors.extend(linker_errors)
                i += 1
                continue

            # Try CMake errors
            cmake_error = self._parse_cmake_line(line, compiler)
            if cmake_error:
                errors.append(cmake_error)
                i += 1
                continue

            # Try MSVC format
            if compiler in ("msvc", "cl"):
                msvc_error = self._parse_msvc_line(line, compiler)
                if msvc_error:
                    errors.append(msvc_error)

            i += 1

        # Deduplicate
        return self._deduplicate(errors)

    def _strip_docker_prefixes(self, output: str) -> str:
        """Remove Docker build progress prefixes from output lines."""
        lines = output.split("\n")
        cleaned = []
        for line in lines:
            # Strip Docker prefix like "#15 0.432 "
            stripped = DOCKER_PREFIX_RE.sub("", line)
            cleaned.append(stripped)
        return "\n".join(cleaned)

    def _parse_gcc_line(self, line: str, compiler: str) -> Optional[BuildError]:
        """Parse a single GCC/Clang error/warning line."""
        match = GCC_ERROR_RE.match(line)
        if not match:
            match = GCC_ERROR_NO_COL_RE.match(line)
            if not match:
                return None

        col = int(match.group("col")) if "col" in match.groupdict() and match.group("col") else 0

        severity_str = match.group("severity")
        severity = {
            "fatal error": BuildErrorSeverity.FATAL,
            "error": BuildErrorSeverity.ERROR,
            "warning": BuildErrorSeverity.WARNING,
            "note": BuildErrorSeverity.NOTE,
        }.get(severity_str, BuildErrorSeverity.ERROR)

        return BuildError(
            id=_next_error_id(),
            file=match.group("file"),
            line=int(match.group("line")),
            column=col,
            severity=severity,
            category=BuildErrorCategory.UNKNOWN,  # Categorized later
            message=match.group("message"),
            raw_output=line,
            compiler=compiler,
        )

    def _parse_linker_line(
        self, line: str, all_lines: list, index: int, compiler: str
    ) -> list[BuildError]:
        """Parse linker error lines."""
        errors = []

        # Undefined reference
        undef_match = LINKER_UNDEF_RE.search(line)
        if undef_match:
            # Try to find source file context
            source_file = "unknown"
            source_line = 0
            loc_match = LINKER_LOCATION_RE.search(line)
            if loc_match:
                source_file = loc_match.group("file")

            # Look backwards for context line
            if index > 0:
                ctx_match = LINKER_CONTEXT_RE.search(all_lines[index - 1])
                if ctx_match:
                    obj_file = ctx_match.group("file")
                    # Convert .o to .cc
                    source_file = obj_file.replace(".o", ".cc")

            errors.append(BuildError(
                id=_next_error_id(),
                file=source_file,
                line=source_line,
                column=0,
                severity=BuildErrorSeverity.ERROR,
                category=BuildErrorCategory.UNDEFINED_REFERENCE,
                message=f"undefined reference to '{undef_match.group('symbol')}'",
                raw_output=line,
                compiler=compiler,
                metadata={"symbol": undef_match.group("symbol")},
            ))
            return errors

        # Multiple definition
        multi_match = LINKER_MULTIDEF_RE.search(line)
        if multi_match:
            errors.append(BuildError(
                id=_next_error_id(),
                file="unknown",
                line=0,
                column=0,
                severity=BuildErrorSeverity.ERROR,
                category=BuildErrorCategory.DUPLICATE_SYMBOL,
                message=f"multiple definition of '{multi_match.group('symbol')}'",
                raw_output=line,
                compiler=compiler,
                metadata={"symbol": multi_match.group("symbol")},
            ))
            return errors

        # Missing library
        lib_match = LINKER_MISSING_LIB_RE.search(line)
        if lib_match:
            errors.append(BuildError(
                id=_next_error_id(),
                file="CMakeLists.txt",
                line=0,
                column=0,
                severity=BuildErrorSeverity.ERROR,
                category=BuildErrorCategory.LINKER_ERROR,
                message=f"cannot find -l{lib_match.group('library')}",
                raw_output=line,
                compiler=compiler,
                metadata={"library": lib_match.group("library")},
            ))
            return errors

        return errors

    def _parse_cmake_line(self, line: str, compiler: str) -> Optional[BuildError]:
        """Parse CMake error/warning lines."""
        match = CMAKE_ERROR_RE.match(line)
        if not match:
            return None

        severity = (
            BuildErrorSeverity.ERROR
            if match.group("severity") == "Error"
            else BuildErrorSeverity.WARNING
        )

        return BuildError(
            id=_next_error_id(),
            file=match.group("file") or "CMakeLists.txt",
            line=int(match.group("line")) if match.group("line") else 0,
            column=0,
            severity=severity,
            category=BuildErrorCategory.CMAKE_ERROR,
            message=match.group("message"),
            raw_output=line,
            compiler="cmake",
            metadata={"command": match.group("command") or ""},
        )

    def _parse_msvc_line(self, line: str, compiler: str) -> Optional[BuildError]:
        """Parse MSVC error/warning lines."""
        match = MSVC_ERROR_RE.match(line)
        if not match:
            return None

        severity_str = match.group("severity")
        severity = {
            "fatal error": BuildErrorSeverity.FATAL,
            "error": BuildErrorSeverity.ERROR,
            "warning": BuildErrorSeverity.WARNING,
            "note": BuildErrorSeverity.NOTE,
        }.get(severity_str, BuildErrorSeverity.ERROR)

        return BuildError(
            id=_next_error_id(),
            file=match.group("file"),
            line=int(match.group("line")),
            column=int(match.group("col")) if match.group("col") else 0,
            severity=severity,
            category=BuildErrorCategory.UNKNOWN,
            message=match.group("message"),
            raw_output=line,
            compiler=compiler,
            metadata={"msvc_code": match.group("code")},
        )

    def _deduplicate(self, errors: list[BuildError]) -> list[BuildError]:
        """Merge duplicate errors, incrementing occurrence_count."""
        seen = {}
        unique = []

        for error in errors:
            key = error.dedup_key()
            if key in seen:
                seen[key].occurrence_count += 1
            else:
                seen[key] = error
                unique.append(error)

        return unique
