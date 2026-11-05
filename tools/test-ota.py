"""Exercise actual OtaService.cpp on a host with simulated HTTP/flash APIs.

Usage: python tools/test-ota.py --cxx <path-to-g++>
No hardware, local password, Wi-Fi secrets or generated project files are read.
"""
import argparse
import os
import pathlib
import shutil
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
CASES = """wrong_password missing_password status page oversize overflow invalid_length
missing_length empty no_partition active_partition small_partition wrong_type chunked
multipart wrong_magic wrong_chip no_hash bootloader begin_failure write_failure
invalid_checksum boot_failure timeout truncated disconnect parser_failure size_mismatch
success split_header max_size""".split()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    args = parser.parse_args()
    compiler = shutil.which(args.cxx)
    if not compiler:
        parser.error("C++ compiler not found; pass --cxx <path-to-g++>")
    environment = os.environ.copy()
    environment["PATH"] = str(pathlib.Path(compiler).parent) + os.pathsep + environment.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="khairc-ota-test-") as directory:
        temp = pathlib.Path(directory)
        # Copy only these source files. The real local credentials are never on
        # the test compiler's include path, even if the user already set them.
        for name in ("OtaService", "OtaPage"):
            for folder, extension in (("src", "cpp"), ("include", "h")):
                shutil.copyfile(ROOT / folder / f"{name}.{extension}", temp / f"{name}.{extension}")
        executable = temp / "test-ota.exe"
        compile_command = [compiler, "-std=c++14", "-Wall", "-Wextra", "-Werror",
                           "-I", str(ROOT / "test/ota_host"), "-I", str(temp),
                           str(ROOT / "test/ota_host/test_ota.cpp"),
                           str(temp / "OtaService.cpp"), str(temp / "OtaPage.cpp"),
                           "-o", str(executable)]
        subprocess.run(compile_command, check=True, env=environment)
        subprocess.run([str(executable), "disabled"], check=True, env=environment)
        (temp / "ota_password.h").write_text(
            '#define HUB_OTA_PASSWORD "test-only-ota-password"\n', encoding="utf-8")
        subprocess.run(compile_command, check=True, env=environment)
        for case in CASES:
            subprocess.run([str(executable), case], check=True, env=environment)
    print(f"Passed {len(CASES) + 1} host scenarios. Hardware OTA still requires board validation.")


if __name__ == "__main__":
    main()
