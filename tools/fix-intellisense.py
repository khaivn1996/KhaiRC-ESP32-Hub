"""Omit missing Arduino SDK include entries from generated IDE configuration."""

import os
from typing import TYPE_CHECKING, Any, Callable

if TYPE_CHECKING:
    # SCons injects these names when executing PlatformIO extra scripts.
    Import: Callable[[str], None]
    env: Any
    projenv: Any

Import("env")

if env.IsIntegrationDump():
    Import("projenv")

    sdk_dir = env.PioPlatform().get_package_dir(
        "framework-arduinoespressif32-esp32s3-libs"
    )
    # Arduino 3.3.12 lists these directories but does not ship them.
    stale_paths = {
        os.path.normcase(os.path.join(sdk_dir, "include", path))
        for path in (
            "esp_system/port/soc",
            "espressif__esp_matter/connectedhomeip/connectedhomeip/src/platform/ESP32/bluedroid",
        )
    }
    for build_env in (env, projenv):
        build_env.Replace(CPPPATH=[
            path for path in build_env.get("CPPPATH", [])
            if os.path.normcase(os.path.abspath(build_env.subst(path))) not in stale_paths
            or os.path.isdir(build_env.subst(path))
        ])
