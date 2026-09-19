"""Guarded import of the compiled pybind11 extension (``flukenorma._core``).

Importing the extension fails if it was never built, or was built for a
different Python version or platform. Every import of it is funnelled
through here so that failure surfaces as a single, actionable ImportError
instead of a bare ``ModuleNotFoundError: flukenorma._core``.
"""

from __future__ import annotations

import importlib
from typing import Any

_CORE_MODULE_NAME = f"{__package__}._core"  # "flukenorma._core"

try:
    core: Any = importlib.import_module(_CORE_MODULE_NAME)
except Exception as e:
    raise ImportError(
        f"Failed to import the compiled extension '{_CORE_MODULE_NAME}'. "
        "Make sure it was built and matches this Python/platform: "
        "run `pip install .` from the repository root, or add the staged "
        "package in the CMake build tree "
        "(e.g. build/windows-msvc/module/bindings/python/Release) to PYTHONPATH."
    ) from e
