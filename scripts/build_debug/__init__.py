"""
WaitingTheLongest Build Debug Agent

Build-time error detection, categorization, auto-fix, and reporting system.
Mirrors the runtime debug system (src/core/debug/) for build-phase use.

Usage:
    python -m scripts.build_debug.build_agent --docker
    python -m scripts.build_debug.build_agent --docker --auto-fix
    python -m scripts.build_debug.build_agent --docker --resume
"""

__version__ = "1.0.0"
__project__ = "WaitingTheLongest"
