"""Metadata lives in pyproject.toml; this shim exists only to force a
platform-specific wheel tag.

fitsolver-client is pure Python, but a built wheel vendors a native
.dll/.so/.dylib under fitsolver_client/_libs/. Without this, setuptools
would tag the wheel `py3-none-any` and pip on Linux would happily install
the Windows binary. Declaring the distribution non-pure makes bdist_wheel
emit `py3-none-<platform>` instead -- one wheel per OS, still valid for
every Python 3.x (ctypes has no ABI dependency).

An editable / sdist install builds no _libs/ dir and this is harmless.
"""

from setuptools import setup
from setuptools.dist import Distribution


class _BinaryDistribution(Distribution):
    def has_ext_modules(self) -> bool:  # noqa: D102
        return True


setup(distclass=_BinaryDistribution)
