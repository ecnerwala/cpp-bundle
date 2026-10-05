import os
import sys

from hatchling.builders.hooks.plugin.interface import BuildHookInterface

# `auditwheel repair` turns this into the manylinux tag of the glibc floor actually required.
DEFAULT_TAG = "py3-none-linux_x86_64"


class CustomBuildHook(BuildHookInterface):
    def initialize(self, version, build_data):
        for binary in ("cpp-bundle", "cpp-minify"):
            if not os.path.isfile(os.path.join(self.root, "dist", binary)):
                sys.exit(f"dist/{binary} missing: build it first (docker build --output type=local,dest=dist .)")
        build_data["pure_python"] = False
        build_data["tag"] = os.environ.get("CPP_BUNDLER_WHEEL_TAG", DEFAULT_TAG)
