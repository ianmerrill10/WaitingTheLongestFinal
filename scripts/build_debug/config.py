"""
Build Debug Agent - Configuration constants and paths.
"""

import os
from pathlib import Path

# Project root (two levels up from this file)
PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent

# Source directories
SRC_DIR = PROJECT_ROOT / "src"
SQL_DIR = PROJECT_ROOT / "sql"
STATIC_DIR = PROJECT_ROOT / "static"
CONFIG_DIR = PROJECT_ROOT / "config"

# Build output
BUILD_DIR = PROJECT_ROOT / "build"
BUILD_LOGS_DIR = PROJECT_ROOT / "build_logs"

# Docker
DOCKERFILE_PATH = PROJECT_ROOT / "Dockerfile"
DOCKER_IMAGE_NAME = "wtl-build-debug"
DOCKER_TARGET_STAGE = "builder"

# CMake
CMAKE_LISTS_PATH = PROJECT_ROOT / "CMakeLists.txt"
CMAKE_BUILD_TYPE = "Release"

# Report output files
ERRORS_JSON = BUILD_LOGS_DIR / "build_errors.json"
REPORT_TXT = BUILD_LOGS_DIR / "build_report.txt"
REPORT_AI_TXT = BUILD_LOGS_DIR / "build_report_ai.txt"
FIXES_JSON = BUILD_LOGS_DIR / "build_fixes.json"
SESSION_MD = BUILD_LOGS_DIR / "BUILD_SESSION.md"
RAW_OUTPUT_DIR = BUILD_LOGS_DIR / "raw_output"

# Session persistence
SESSION_FILE = BUILD_LOGS_DIR / "session_state.json"

# Project conventions (from CODING_STANDARDS.md)
NAMESPACE_ROOT = "wtl"
NAMESPACE_HIERARCHY = {
    "core": "wtl::core",
    "core::utils": "wtl::core::utils",
    "core::db": "wtl::core::db",
    "core::debug": "wtl::core::debug",
    "core::auth": "wtl::core::auth",
    "core::services": "wtl::core::services",
    "core::models": "wtl::core::models",
    "core::controllers": "wtl::core::controllers",
    "core::websocket": "wtl::core::websocket",
    "modules": "wtl::modules",
    "content::tiktok": "wtl::content::tiktok",
    "content::blog": "wtl::content::blog",
    "content::social": "wtl::content::social",
    "content::media": "wtl::content::media",
    "content::templates": "wtl::content::templates",
    "analytics": "wtl::analytics",
    "notifications": "wtl::notifications",
    "aggregators": "wtl::aggregators",
    "workers": "wtl::workers",
    "clients": "wtl::clients",
    "admin": "wtl::admin",
}

# C++ source extensions
CPP_SOURCE_EXTENSIONS = {".cc", ".cpp", ".cxx", ".c++"}
CPP_HEADER_EXTENSIONS = {".h", ".hpp", ".hxx", ".h++"}

# Max errors to store in memory
MAX_ERRORS_IN_MEMORY = 5000

# Default retry/build settings
DEFAULT_MAX_ATTEMPTS = 1
DEFAULT_BUILD_TIMEOUT_SECONDS = 1800  # 30 minutes

# Docker progress prefix pattern
DOCKER_PREFIX_PATTERN = r"^#\d+\s+(?:\[\w+\s+\d+/\d+\]\s+)?"

# Compiler identification
COMPILERS = {
    "gcc": "GNU C++ Compiler",
    "g++": "GNU C++ Compiler",
    "clang": "Clang C++ Compiler",
    "clang++": "Clang C++ Compiler",
    "cl": "Microsoft Visual C++",
    "msvc": "Microsoft Visual C++",
}


def ensure_dirs():
    """Create required output directories."""
    BUILD_LOGS_DIR.mkdir(parents=True, exist_ok=True)
    RAW_OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
