"""Build hook: the wheel carries native libraries, so it is not a pure Python wheel.

Two things are overridden here:
* ``has_ext_modules`` makes setuptools treat the package as platform-specific, even
  though the native code is loaded through ctypes rather than an extension module.
* ``get_tag`` reports ``py3-none-<platform>``: ctypes only needs a Python 3 ABI, so one
  wheel per platform covers every interpreter version.
"""

from setuptools import Distribution, setup

try:
    from setuptools.command.bdist_wheel import bdist_wheel
except ImportError:
    from wheel.bdist_wheel import bdist_wheel


class BinaryDistribution(Distribution):
    def has_ext_modules(self):
        return True


class NativeWheel(bdist_wheel):
    def get_tag(self):
        _, _, platform_tag = super().get_tag()
        return "py3", "none", platform_tag


setup(distclass=BinaryDistribution, cmdclass={"bdist_wheel": NativeWheel})
