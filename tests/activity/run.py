#!/usr/bin/env python3
"""Test the complete RPC handler with real nanopb messages and stubbed ZMK APIs."""

import argparse
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path,
                        default=root / "src/studio/settings_rpc_handler.c",
                        help="override the handler source for regression reproduction")
    args = parser.parse_args()

    if "NANOPB_SOURCE" in os.environ:
        nanopb = Path(os.environ["NANOPB_SOURCE"]).resolve()
    else:
        nanopb = Path(subprocess.check_output(
            ["west", "list", "nanopb", "-f", "{abspath}"], cwd=root, text=True).strip())
    for name in ("pb_common.c", "pb_encode.c", "pb_decode.c", "generator/nanopb_generator.py"):
        if not (nanopb / name).is_file():
            parser.error(f"missing nanopb dependency: {nanopb / name}")
    source = args.source.resolve()
    if not source.is_file():
        parser.error(f"missing handler source: {source}")

    with tempfile.TemporaryDirectory(prefix="settings-rpc-") as directory:
        work = Path(directory)
        # The generator may regenerate its own Python proto. Keep dependency
        # checkouts read-only and avoid relying on pre-existing generated files.
        shutil.copytree(nanopb / "generator", work / "generator")
        subprocess.run([
            sys.executable, str(work / "generator/nanopb_generator.py"),
            "-I", str(root / "proto"), "-D", str(work),
            str(root / "proto/zmk/settings/core.proto"),
        ], check=True, timeout=60)
        for name in ("zephyr/kernel.h", "zephyr/logging/log.h", "zmk/activity.h",
                     "zmk/event_manager.h", "zmk/studio/custom.h"):
            header = work / name
            header.parent.mkdir(parents=True, exist_ok=True)
            header.write_text("/* ZMK interfaces supplied by the host harness. */\n")

        variants = {
            "optimized": ["-O2"],
            "sanitized": ["-O1", "-g", "-fno-omit-frame-pointer",
                          "-fsanitize=address,undefined", "-fno-sanitize-recover=all"],
        }
        for role, split, central, relay, studio in (("standalone", 0, 0, 0, 1),
                                                    ("central", 1, 1, 1, 1),
                                                    ("central-no-relay", 1, 1, 0, 1),
                                                    ("peripheral", 1, 0, 1, 0)):
            for label, flags in variants.items():
                binary = work / f"activity-{role}-{label}"
                subprocess.run([
                    *shlex.split(os.environ.get("CC", "cc")),
                    "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-Wno-unused-parameter", "-Wno-unused-function", *flags,
                    "-DPB_WITHOUT_64BIT=1", f"-DCONFIG_ZMK_SPLIT={split}",
                    f"-DCONFIG_ZMK_SPLIT_RELAY_EVENT={relay}",
                    f"-DCONFIG_ZMK_SPLIT_ROLE_CENTRAL={central}",
                    f"-DTEST_RPC={studio}", f'-DTEST_ROLE="{role}"',
                    f'-DCHANGED_SOURCE="{root / "src/events/activity_settings_changed.c"}"',
                    f'-DREPORT_SOURCE="{root / "src/events/activity_settings_report.c"}"',
                    f'-DRPC_SOURCE="{source}"', f'-DTEST_VARIANT="{label}"',
                    f"-I{nanopb}", f"-I{work}", f"-I{root / 'include'}",
                    str(root / "tests/activity/handler_test.c"),
                    str(work / "zmk/settings/core.pb.c"),
                    *(str(nanopb / f"pb_{name}.c") for name in ("common", "encode", "decode")),
                    "-o", str(binary),
                ], check=True, timeout=60)
                subprocess.run([str(binary)], check=True, timeout=20,
                               env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})


if __name__ == "__main__":
    main()
