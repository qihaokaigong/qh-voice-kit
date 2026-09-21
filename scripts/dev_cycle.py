#!/usr/bin/env python3

"""Fast, guarded app-only flashing for an identified development board."""

from __future__ import annotations

import argparse
import hashlib
import json
import shlex
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Callable, Sequence


class DevCycleError(ValueError):
    pass


@dataclass(frozen=True)
class BuildArtifact:
    role: str
    address: int
    path: Path
    size: int
    sha256: str


ROLE_BY_FILENAME = {
    "boot_app0.bin": "ota-data",
}


def _digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _role_for(filename: str) -> str:
    if filename in ROLE_BY_FILENAME:
        return ROLE_BY_FILENAME[filename]
    if filename.endswith(".bootloader.bin"):
        return "bootloader"
    if filename.endswith(".partitions.bin"):
        return "partition-table"
    if filename.endswith(".ino.bin") and not filename.endswith(".merged.bin"):
        return "app"
    raise DevCycleError(f"unexpected build artifact: {filename}")


def _build_artifacts(build_root: Path) -> dict[str, BuildArtifact]:
    flash_args = build_root / "flash_args"
    try:
        lines = flash_args.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise DevCycleError(f"cannot read build flash_args: {error}") from error
    artifacts: dict[str, BuildArtifact] = {}
    for line in lines:
        fields = line.split()
        if len(fields) != 2 or not fields[0].startswith("0x"):
            continue
        try:
            address = int(fields[0], 16)
        except ValueError as error:
            raise DevCycleError("flash_args contains an invalid address") from error
        path = (build_root / fields[1]).resolve()
        if path.parent != build_root.resolve() or not path.is_file():
            raise DevCycleError("flash_args artifact is missing or escapes build root")
        role = _role_for(path.name)
        if role in artifacts:
            raise DevCycleError(f"duplicate build artifact role: {role}")
        artifacts[role] = BuildArtifact(
            role=role,
            address=address,
            path=path,
            size=path.stat().st_size,
            sha256=_digest(path),
        )
    if set(artifacts) != {"bootloader", "partition-table", "ota-data", "app"}:
        raise DevCycleError("flash_args does not contain the exact firmware artifact set")
    return artifacts


def _load_json(path: Path, label: str) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise DevCycleError(f"cannot read {label}: {error}") from error
    if not isinstance(value, dict):
        raise DevCycleError(f"{label} must be a JSON object")
    return value


def build_plan(
    build_root: Path,
    baseline_manifest: Path,
    hardware_profile: Path,
    confirmed_profile_id: str,
    port: str,
    esptool: str,
) -> dict:
    profile = _load_json(hardware_profile, "hardware profile")
    profile_id = profile.get("profileId")
    if not isinstance(profile_id, str) or profile_id != confirmed_profile_id:
        raise DevCycleError("hardware profile confirmation does not match profile file")
    baseline = _load_json(baseline_manifest, "baseline manifest")
    if baseline.get("hardwareProfileId") != profile_id:
        raise DevCycleError("baseline manifest targets a different hardware profile")
    flash = baseline.get("flash")
    if not isinstance(flash, dict) or flash.get("eraseAll") is not False:
        raise DevCycleError("baseline manifest must preserve flash configuration")
    baseline_files = flash.get("files")
    if not isinstance(baseline_files, list):
        raise DevCycleError("baseline manifest flash.files must be a list")
    baseline_by_role = {
        item.get("role"): item for item in baseline_files if isinstance(item, dict)
    }
    if set(baseline_by_role) != {
        "bootloader",
        "partition-table",
        "ota-data",
        "app",
    }:
        raise DevCycleError("baseline manifest does not contain the exact artifact set")

    build = _build_artifacts(build_root)
    for role in ("bootloader", "partition-table", "ota-data"):
        actual = build[role]
        expected = baseline_by_role[role]
        if (
            expected.get("address") != actual.address
            or expected.get("size") != actual.size
            or expected.get("sha256") != actual.sha256
        ):
            raise DevCycleError(f"{role} changed; app-only development flash refused")
    app = build["app"]
    if baseline_by_role["app"].get("address") != app.address:
        raise DevCycleError("app address changed; app-only development flash refused")
    if not port.strip():
        raise DevCycleError("serial port must be non-empty")

    command = [
        esptool,
        "--chip",
        "esp32s3",
        "--port",
        port,
        "--baud",
        "921600",
        "--before",
        "default-reset",
        "--after",
        "hard-reset",
        "write-flash",
        "--flash-mode",
        "keep",
        "--flash-freq",
        "keep",
        "--flash-size",
        "keep",
        hex(app.address),
        str(app.path),
    ]
    return {
        "status": "dev_plan_ready",
        "hardwareProfileId": profile_id,
        "baselineReleaseId": baseline.get("releaseId"),
        "port": port,
        "flashMode": "app-only",
        "eraseAll": False,
        "appAddress": hex(app.address),
        "appSize": app.size,
        "appSha256": app.sha256,
        "command": command,
    }


def _run_logged(
    stage: str,
    command: Sequence[str],
    repo_root: Path,
    log_file,
    runner: Callable = subprocess.run,
) -> None:
    log_file.write(f"$ {shlex.join(str(item) for item in command)}\n")
    log_file.flush()
    completed = runner(
        list(command),
        cwd=repo_root,
        stdout=log_file,
        stderr=subprocess.STDOUT,
        text=True,
    )
    if completed.returncode != 0:
        raise DevCycleError(f"{stage} failed; inspect the development log")


def run_dev_cycle(
    *,
    repo_root: Path,
    build_root: Path,
    baseline_manifest: Path,
    hardware_profile: Path,
    confirmed_profile_id: str,
    port: str,
    esptool: str,
    test_modules: Sequence[str],
    log_path: Path,
    runner: Callable = subprocess.run,
) -> dict:
    """Test, incrementally compile, and write only the app partition."""
    repo_root = repo_root.resolve()
    log_path = log_path.resolve()
    log_path.parent.mkdir(parents=True, exist_ok=True)
    test_command = [sys.executable, "-m", "unittest"]
    if test_modules:
        test_command.extend(["-v", *test_modules])
    else:
        test_command.extend(["discover", "-s", "tests", "-v"])

    with log_path.open("w", encoding="utf-8") as log_file:
        _run_logged("host tests", test_command, repo_root, log_file, runner)
        _run_logged(
            "incremental compile",
            [str(repo_root / "scripts" / "compile.sh")],
            repo_root,
            log_file,
            runner,
        )
        plan = build_plan(
            build_root,
            baseline_manifest,
            hardware_profile,
            confirmed_profile_id,
            port,
            esptool,
        )
        _run_logged("app-only flash", plan["command"], repo_root, log_file, runner)

    return {
        **plan,
        "status": "dev_flash_complete",
        "logPath": str(log_path),
    }


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--plan-only", action="store_true")
    action.add_argument("--apply", action="store_true")
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--baseline-manifest", type=Path, required=True)
    parser.add_argument("--hardware-profile", type=Path, required=True)
    parser.add_argument("--confirm-hardware-profile-id", required=True)
    parser.add_argument("--port", required=True)
    parser.add_argument("--esptool", default="esptool")
    parser.add_argument("--test-module", action="append", default=[])
    parser.add_argument("--log-path", type=Path)
    parser.add_argument("--json", action="store_true")
    return parser


def main() -> int:
    arguments = _parser().parse_args()
    repo_root = Path(__file__).resolve().parents[1]
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    log_path = arguments.log_path or repo_root / ".build" / "dev-logs" / f"{timestamp}.log"
    try:
        if arguments.plan_only:
            result = build_plan(
                arguments.build_root,
                arguments.baseline_manifest,
                arguments.hardware_profile,
                arguments.confirm_hardware_profile_id,
                arguments.port,
                arguments.esptool,
            )
        else:
            result = run_dev_cycle(
                repo_root=repo_root,
                build_root=arguments.build_root,
                baseline_manifest=arguments.baseline_manifest,
                hardware_profile=arguments.hardware_profile,
                confirmed_profile_id=arguments.confirm_hardware_profile_id,
                port=arguments.port,
                esptool=arguments.esptool,
                test_modules=arguments.test_module,
                log_path=log_path,
            )
    except DevCycleError as error:
        payload = {
            "status": "dev_cycle_blocked",
            "message": str(error),
            "logPath": str(log_path.resolve()) if arguments.apply else None,
        }
        print(json.dumps(payload, ensure_ascii=False) if arguments.json else error,
              file=sys.stderr)
        return 2
    print(json.dumps(result, ensure_ascii=False) if arguments.json else result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
