"""
Build Debug Agent - Local CMake build runner (fallback).

Runs the C++ build locally via CMake when Docker is not available.
"""

import subprocess
import time
import os
from pathlib import Path
from datetime import datetime
from . import config


class CMakeBuilder:
    """Runs the build locally via CMake, captures output."""

    def __init__(
        self,
        project_root: str = None,
        build_dir: str = None,
        build_type: str = None,
    ):
        self.project_root = Path(project_root) if project_root else config.PROJECT_ROOT
        self.build_dir = Path(build_dir) if build_dir else config.BUILD_DIR
        self.build_type = build_type or config.CMAKE_BUILD_TYPE

    def check_cmake_available(self) -> tuple[bool, str]:
        """Check if CMake is installed."""
        try:
            result = subprocess.run(
                ["cmake", "--version"],
                capture_output=True,
                text=True,
                timeout=10,
            )
            if result.returncode == 0:
                version = result.stdout.strip().split("\n")[0]
                return True, version
            return False, "CMake returned non-zero"
        except FileNotFoundError:
            return False, "CMake not found in PATH"
        except subprocess.TimeoutExpired:
            return False, "CMake version check timed out"

    def check_compiler_available(self) -> tuple[bool, str]:
        """Check if a C++ compiler is available."""
        for compiler in ["g++", "clang++", "cl"]:
            try:
                result = subprocess.run(
                    [compiler, "--version"],
                    capture_output=True,
                    text=True,
                    timeout=10,
                )
                if result.returncode == 0:
                    version = result.stdout.strip().split("\n")[0]
                    return True, f"{compiler}: {version}"
            except (FileNotFoundError, subprocess.TimeoutExpired):
                continue
        return False, "No C++ compiler found (tried g++, clang++, cl)"

    def configure(self) -> tuple[int, str]:
        """Run CMake configuration step."""
        self.build_dir.mkdir(parents=True, exist_ok=True)

        cmd = [
            "cmake",
            f"-DCMAKE_BUILD_TYPE={self.build_type}",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            str(self.project_root),
        ]

        print(f"[BuildAgent] Configuring: cmake -B {self.build_dir} ...")

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=120,
                cwd=str(self.build_dir),
            )
            output = result.stdout + "\n" + result.stderr
            return result.returncode, output

        except subprocess.TimeoutExpired:
            return -1, "CMake configuration timed out"

    def build(self, jobs: int = 0) -> tuple[int, str, float]:
        """
        Run cmake --build.

        Args:
            jobs: Number of parallel jobs. 0 = auto-detect.

        Returns: (exit_code, raw_output, duration_seconds)
        """
        if not (self.build_dir / "CMakeCache.txt").exists():
            print("[BuildAgent] No CMakeCache.txt found, running configure first...")
            cfg_code, cfg_output = self.configure()
            if cfg_code != 0:
                return cfg_code, cfg_output, 0.0

        if jobs <= 0:
            jobs = os.cpu_count() or 4

        cmd = [
            "cmake",
            "--build", str(self.build_dir),
            f"-j{jobs}",
            "--", "VERBOSE=1",
        ]

        build_command = " ".join(cmd)
        print(f"[BuildAgent] Building: {build_command}")
        print(f"[BuildAgent] This may take several minutes...")

        start_time = time.time()

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=config.DEFAULT_BUILD_TIMEOUT_SECONDS,
                cwd=str(self.project_root),
            )

            duration = time.time() - start_time
            raw_output = result.stdout + "\n" + result.stderr

            # Save raw output
            self._save_raw_output(raw_output, duration, result.returncode)

            return result.returncode, raw_output, duration

        except subprocess.TimeoutExpired:
            duration = time.time() - start_time
            return -1, f"Build timed out after {duration:.0f}s", duration

    def clean(self):
        """Remove build directory."""
        import shutil
        if self.build_dir.exists():
            shutil.rmtree(self.build_dir, ignore_errors=True)
            print(f"[BuildAgent] Cleaned build directory: {self.build_dir}")

    def _save_raw_output(self, output: str, duration: float, exit_code: int):
        """Save raw build output to file."""
        config.ensure_dirs()
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"cmake_build_{timestamp}.log"
        filepath = config.RAW_OUTPUT_DIR / filename

        header = (
            f"# CMake Build Output\n"
            f"# Time: {datetime.now().isoformat()}\n"
            f"# Exit Code: {exit_code}\n"
            f"# Duration: {duration:.1f}s\n"
            f"# Build Type: {self.build_type}\n"
            f"# Build Dir: {self.build_dir}\n"
            f"{'=' * 80}\n\n"
        )

        filepath.write_text(header + output, encoding="utf-8")
        print(f"[BuildAgent] Raw output saved to: {filepath}")
