"""
Build Debug Agent - Docker build runner.

Runs the C++ build inside Docker using the existing Dockerfile's builder stage.
Captures full compiler output for error parsing.
"""

import subprocess
import time
from pathlib import Path
from datetime import datetime
from . import config


class DockerBuilder:
    """Runs the build inside Docker, captures output."""

    def __init__(
        self,
        project_root: str = None,
        dockerfile: str = None,
        image_name: str = None,
        target_stage: str = None,
    ):
        self.project_root = Path(project_root) if project_root else config.PROJECT_ROOT
        self.dockerfile = dockerfile or str(config.DOCKERFILE_PATH)
        self.image_name = image_name or config.DOCKER_IMAGE_NAME
        self.target_stage = target_stage or config.DOCKER_TARGET_STAGE

    def check_docker_available(self) -> tuple[bool, str]:
        """Check if Docker is installed and running."""
        try:
            result = subprocess.run(
                ["docker", "info"],
                capture_output=True,
                text=True,
                timeout=15,
            )
            if result.returncode == 0:
                return True, "Docker is available and running"
            return False, f"Docker not responding: {result.stderr.strip()}"
        except FileNotFoundError:
            return False, "Docker not found in PATH"
        except subprocess.TimeoutExpired:
            return False, "Docker info command timed out"

    def build(self, no_cache: bool = True) -> tuple[int, str, float]:
        """
        Execute Docker build targeting the builder stage.

        Uses --progress=plain for full compiler output visibility.
        Uses --target builder to stop after compilation (no runtime stage).

        Returns: (exit_code, raw_output, duration_seconds)
        """
        cmd = [
            "docker", "build",
            "--target", self.target_stage,
            "--progress=plain",
        ]

        if no_cache:
            cmd.append("--no-cache")

        cmd.extend([
            "-t", f"{self.image_name}:latest",
            "-f", self.dockerfile,
            str(self.project_root),
        ])

        build_command = " ".join(cmd)
        print(f"[BuildAgent] Running: {build_command}")
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

            # Docker sends build output to stderr with --progress=plain
            raw_output = result.stdout + "\n" + result.stderr

            # Save raw output
            self._save_raw_output(raw_output, duration, result.returncode)

            return result.returncode, raw_output, duration

        except subprocess.TimeoutExpired:
            duration = time.time() - start_time
            msg = f"[BuildAgent] Build timed out after {duration:.0f}s"
            print(msg)
            return -1, msg, duration

    def build_with_verbose_errors(self, no_cache: bool = True) -> tuple[int, str, float]:
        """
        Build with extra verbose error output.
        Sets CMAKE_VERBOSE_MAKEFILE=ON for detailed compilation commands.
        """
        # Create a modified build command with verbose cmake
        cmd = [
            "docker", "build",
            "--target", self.target_stage,
            "--progress=plain",
            "--build-arg", "CMAKE_ARGS=-DCMAKE_VERBOSE_MAKEFILE=ON",
        ]

        if no_cache:
            cmd.append("--no-cache")

        cmd.extend([
            "-t", f"{self.image_name}:verbose",
            "-f", self.dockerfile,
            str(self.project_root),
        ])

        print(f"[BuildAgent] Running verbose build...")
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
            self._save_raw_output(raw_output, duration, result.returncode, suffix="_verbose")

            return result.returncode, raw_output, duration

        except subprocess.TimeoutExpired:
            duration = time.time() - start_time
            return -1, f"Verbose build timed out after {duration:.0f}s", duration

    def clean(self):
        """Remove build debug images."""
        for tag in ["latest", "verbose"]:
            try:
                subprocess.run(
                    ["docker", "rmi", f"{self.image_name}:{tag}"],
                    capture_output=True,
                    timeout=30,
                )
            except (subprocess.TimeoutExpired, FileNotFoundError):
                pass

    def _save_raw_output(
        self, output: str, duration: float, exit_code: int, suffix: str = ""
    ):
        """Save raw build output to file for debugging."""
        config.ensure_dirs()
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"docker_build_{timestamp}{suffix}.log"
        filepath = config.RAW_OUTPUT_DIR / filename

        header = (
            f"# Docker Build Output\n"
            f"# Time: {datetime.now().isoformat()}\n"
            f"# Exit Code: {exit_code}\n"
            f"# Duration: {duration:.1f}s\n"
            f"# Image: {self.image_name}\n"
            f"# Target: {self.target_stage}\n"
            f"{'=' * 80}\n\n"
        )

        filepath.write_text(header + output, encoding="utf-8")
        print(f"[BuildAgent] Raw output saved to: {filepath}")
