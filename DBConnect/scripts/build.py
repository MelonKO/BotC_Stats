#!/usr/bin/env python3
"""Configure and build DBConnect outside an IDE.

The script prepares a Visual Studio developer environment (so that ``cl.exe``
and Ninja are available), points ``VCPKG_ROOT`` at the standalone vcpkg
checkout instead of the copy shipped with Visual Studio, and then forwards the
work to the shared CMake presets from ``CMakePresets.json``.

Usage (from the repository root or from ``DBConnect``):

    python DBConnect/scripts/build.py doctor      # show toolchain/vcpkg state
    python DBConnect/scripts/build.py             # configure + build Debug
    python DBConnect/scripts/build.py build       # same, explicit
    python DBConnect/scripts/build.py build --preset release
    python DBConnect/scripts/build.py deploy      # configure Release + deploy
    python DBConnect/scripts/build.py clean       # remove a build directory

Tool paths can be pinned in ``DBConnect/scripts/build.ini`` (git-ignored, see
``build.ini.example``). Resolution order for every tool is: command line
option, then the config file, then the environment (``VCPKG_ROOT``), then
auto-detection.

The script never deletes ``vcpkg_installed``: vcpkg recomputes the ABI of every
port from the compiler, triplet and installed tree, so removing or truncating
that directory forces a full rebuild of Qt and its dependencies.

Only the Python standard library is used.
"""

from __future__ import annotations

import argparse
import configparser
import json
import locale
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
SOURCE_DIR = SCRIPT_DIR.parent
PRESETS_FILE = SOURCE_DIR / "CMakePresets.json"
VCPKG_INSTALLED = SOURCE_DIR / "vcpkg_installed"
CONFIG_FILE = SCRIPT_DIR / "build.ini"
BUILD_DIRS = {
    "debug": SOURCE_DIR / "build_debug",
    "release": SOURCE_DIR / "build_release",
}
DEFAULT_VCPKG_ROOT = Path("C:/vcpkg")
MIN_CMAKE = (3, 25, 0)
VCPKG_TOOLCHAIN = Path("scripts/buildsystems/vcpkg.cmake")


class ScriptError(RuntimeError):
    """A recoverable error that should be reported without a traceback."""


def log(message: str) -> None:
    print(message, flush=True)


def warn(message: str) -> None:
    print(f"[warn] {message}", flush=True)


def fail(message: str) -> "None":
    raise ScriptError(message)


# ---------------------------------------------------------------------------
# Configuration file
# ---------------------------------------------------------------------------

class Config:
    """Settings from ``build.ini`` plus the path they were read from."""

    def __init__(self, path: Path, parser: configparser.ConfigParser, exists: bool) -> None:
        self.path = path
        self.parser = parser
        self.exists = exists

    def get(self, section: str, option: str) -> str | None:
        try:
            value = self.parser.get(section, option, fallback="").strip()
        except configparser.Error:
            return None
        return value or None

    def get_int(self, section: str, option: str) -> int | None:
        value = self.get(section, option)
        if value is None:
            return None
        try:
            return int(value)
        except ValueError:
            warn(f"config {self.path.name}: [{section}] {option} = {value!r} is not a number, ignoring it")
            return None

    @property
    def preset(self) -> str | None:
        return self.get("build", "preset")

    @property
    def jobs(self) -> int | None:
        return self.get_int("build", "jobs")


def load_config(explicit: str | None) -> Config:
    """Read the tool configuration; a missing local file is not an error."""
    if explicit:
        path = Path(explicit).expanduser()
        if not path.is_file():
            fail(f"config file not found: {path}")
    else:
        from_env = os.environ.get("DBCT_BUILD_CONFIG")
        path = Path(from_env).expanduser() if from_env else CONFIG_FILE

    exists = path.is_file()
    parser = configparser.ConfigParser()
    if exists:
        try:
            with path.open(encoding="utf-8") as handle:
                parser.read_file(handle)
        except (OSError, configparser.Error) as error:
            fail(f"cannot read {path}: {error}")
    return Config(path, parser, exists)


# ---------------------------------------------------------------------------
# Toolchain discovery
# ---------------------------------------------------------------------------

def locale_encoding() -> str:
    """Console code page encoding used by Visual Studio and CMake on Windows."""
    if os.name == "nt":
        return "mbcs"
    return locale.getpreferredencoding(False)


def command_output(command: list[str], env: dict[str, str] | None = None) -> str:
    completed = subprocess.run(
        command,
        capture_output=True,
        text=True,
        encoding=locale_encoding(),
        errors="replace",
        env=env,
    )
    return f"{completed.stdout}\n{completed.stderr}"


def autodetect_vs_install() -> Path | None:
    """Return the newest Visual Studio installation that provides the C++ toolset."""
    vswhere = Path(
        os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    ) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if vswhere.is_file():
        completed = subprocess.run(
            [
                str(vswhere),
                "-latest",
                "-products",
                "*",
                "-requires",
                "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                "-property",
                "installationPath",
            ],
            capture_output=True,
            text=True,
            errors="replace",
        )
        candidates = completed.stdout.strip().splitlines()
        if candidates:
            path = Path(candidates[0].strip())
            if (path / "Common7" / "Tools" / "VsDevCmd.bat").is_file():
                return path

    base = Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "Microsoft Visual Studio"
    for bat in sorted(base.glob("**/Common7/Tools/VsDevCmd.bat"), reverse=True):
        return bat.parents[2]
    return None


def resolve_vs_install(explicit: str | None, config: Config) -> tuple[Path | None, str]:
    """Locate Visual Studio: command line, config file, then vswhere."""
    candidates: list[tuple[Path, str]] = []
    if explicit:
        candidates.append((Path(explicit), "command line"))
    configured = config.get("toolchain", "vs_install")
    if configured:
        candidates.append((Path(configured), f"config file {config.path.name}"))

    for path, source in candidates:
        if (path / "Common7" / "Tools" / "VsDevCmd.bat").is_file():
            return path, source
        if source == "command line":
            fail(f"--vs-install does not contain Common7/Tools/VsDevCmd.bat: {path}")
        warn(f"configured vs_install is not a Visual Studio installation: {path}")

    detected = autodetect_vs_install()
    return (detected, "auto-detected") if detected else (None, "not found")


def developer_environment(vs_install: Path) -> dict[str, str]:
    """Import the environment produced by ``VsDevCmd.bat`` (x64 host and target)."""
    dev_cmd = vs_install / "Common7" / "Tools" / "VsDevCmd.bat"
    if not dev_cmd.is_file():
        fail(f"VsDevCmd.bat not found in {vs_install}")

    command = f'cmd.exe /c call "{dev_cmd}" -arch=amd64 -host_arch=amd64 -no_logo && set'
    completed = subprocess.run(
        command,
        capture_output=True,
        text=True,
        encoding=locale_encoding(),
        errors="replace",
    )
    if completed.returncode != 0:
        output = (completed.stdout or "")[-2000:] + (completed.stderr or "")[-2000:]
        fail(f"VsDevCmd.bat failed with exit code {completed.returncode}:\n{output}")

    environment: dict[str, str] = {}
    for raw_line in completed.stdout.splitlines():
        name, separator, value = raw_line.partition("=")
        if separator and name and " " not in name.strip():
            # Visual Studio and cmd.exe write names as "Path", not "PATH".
            environment[name.strip().upper()] = value
    if "PATH" not in environment:
        fail("could not read the Visual Studio environment (PATH is missing)")
    return environment


def cmake_version(cmake: Path, env: dict[str, str]) -> tuple[int, ...] | None:
    try:
        output = command_output([str(cmake), "--version"], env)
    except OSError:
        return None
    match = re.search(r"cmake version (\d+)\.(\d+)\.(\d+)", output)
    if not match:
        return None
    return tuple(int(part) for part in match.groups())


def cmake_candidates(explicit: str | None, config: Config) -> list[tuple[Path, str]]:
    """Ordered CMake candidates with the place each one came from."""
    candidates: list[tuple[Path, str]] = []
    if explicit:
        candidates.append((Path(explicit), "command line"))
    configured = config.get("toolchain", "cmake")
    if configured:
        candidates.append((Path(configured), f"config file {config.path.name}"))
    candidates.append(
        (Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "CMake" / "bin" / "cmake.exe",
         "auto-detected")
    )
    on_path = shutil.which("cmake")
    if on_path:
        candidates.append((Path(on_path), "PATH"))
    candidates.append((Path(r"C:\msys64\mingw64\bin\cmake.exe"), "auto-detected"))

    unique: list[tuple[Path, str]] = []
    seen: list[Path] = []
    for path, source in candidates:
        expanded = path.expanduser()
        if expanded not in seen:
            seen.append(expanded)
            unique.append((expanded, source))
    return unique


def resolve_cmake(explicit: str | None, env: dict[str, str], config: Config) -> tuple[Path, tuple[int, ...], str]:
    """Pick a CMake executable and keep it stable between runs.

    vcpkg hashes the compiler detection performed by CMake into every port ABI,
    so switching between the CMake from Visual Studio, MSYS2 and an IDE
    triggers a full reinstall of the dependencies.
    """
    problems: list[str] = []
    for candidate, source in cmake_candidates(explicit, config):
        if not candidate.is_file():
            problems.append(f"{candidate} ({source}): not found")
            continue
        version = cmake_version(candidate, env)
        if version is None:
            problems.append(f"{candidate} ({source}): unusable")
            continue
        if version < MIN_CMAKE:
            problems.append(
                f"{candidate} ({source}): {'.'.join(map(str, version))} is older than 3.25"
            )
            continue
        return candidate, version, source

    detail = "\n".join(f"  - {problem}" for problem in problems)
    fail(f"no suitable CMake found (3.25+ required):\n{detail}")


def resolve_vcpkg_root(explicit: str | None, config: Config) -> tuple[Path, str]:
    """Locate the standalone vcpkg checkout."""
    candidates: list[tuple[Path, str]] = []
    if explicit:
        candidates.append((Path(explicit), "command line"))
    configured = config.get("toolchain", "vcpkg_root")
    if configured:
        candidates.append((Path(configured), f"config file {config.path.name}"))
    from_env = os.environ.get("VCPKG_ROOT")
    if from_env:
        candidates.append((Path(from_env), "environment VCPKG_ROOT"))
    candidates.append((DEFAULT_VCPKG_ROOT, "default"))

    tried: list[str] = []
    for candidate, source in candidates:
        if (candidate / VCPKG_TOOLCHAIN).is_file():
            return candidate.resolve(), source
        tried.append(f"  - {candidate} ({source}): missing {VCPKG_TOOLCHAIN}")
    fail("no usable vcpkg checkout found:\n" + "\n".join(tried))


def resolve_ninja(config: Config, env: dict[str, str], vs_install: Path | None) -> tuple[Path | None, str]:
    """Locate Ninja: config file, then the copy shipped with Visual Studio, then PATH."""
    configured = config.get("toolchain", "ninja")
    if configured:
        path = Path(configured).expanduser()
        if path.is_file():
            return path, f"config file {config.path.name}"
        warn(f"configured ninja was not found: {path}")

    if vs_install is not None:
        bundled = vs_install / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "Ninja" / "ninja.exe"
        if bundled.is_file():
            return bundled, "Visual Studio"

    on_path = shutil.which("ninja.exe", path=env.get("PATH", ""))
    if on_path:
        return Path(on_path), "PATH"
    return None, "not found"


# ---------------------------------------------------------------------------
# Environment preparation
# ---------------------------------------------------------------------------

def prepend_ninja(environment: dict[str, str], ninja: Path | None) -> None:
    """Put the selected Ninja first on PATH.

    MSYS2 also ships Ninja; picking it up or not depending on the active
    terminal makes builds non-reproducible.
    """
    if ninja is None or not ninja.is_file():
        return
    environment["PATH"] = os.pathsep.join(
        [str(ninja.parent), environment.get("PATH", os.environ.get("PATH", ""))]
    )


def prepare_environment(args: argparse.Namespace) -> tuple[dict[str, str], Path, Path, tuple[int, ...]]:
    config = load_config(args.config)
    vs_install, vs_source = resolve_vs_install(args.vs_install, config)

    if args.no_devshell:
        environment = dict(os.environ)
        toolset_source = "current environment (--no-devshell)"
    elif shutil.which("cl.exe"):
        # Already inside a developer prompt: reuse it, but still normalise VCPKG_ROOT.
        environment = dict(os.environ)
        toolset_source = "current environment"
    else:
        if vs_install is None:
            fail("Visual Studio with the C++ toolset was not found; set vs_install in "
                 f"{CONFIG_FILE.name}, pass --vs-install or run from a Developer PowerShell")
        environment = developer_environment(vs_install)
        toolset_source = f"{vs_install} ({vs_source})"

    ninja, ninja_source = resolve_ninja(config, environment, vs_install)
    prepend_ninja(environment, ninja)

    vcpkg_root, vcpkg_source = resolve_vcpkg_root(args.vcpkg_root, config)
    # Visual Studio's vcpkg.bat sets VCPKG_ROOT to the bundled copy; restore ours.
    environment["VCPKG_ROOT"] = str(vcpkg_root)

    cmake, version, cmake_source = resolve_cmake(args.cmake, environment, config)
    environment.setdefault("CMAKE_EXPORT_COMPILE_COMMANDS", "ON")

    log(f"[env] config: {config.path}{'' if config.exists else ' (not found, using defaults)'}")
    log(f"[env] toolset: {toolset_source}")
    log(f"[env] VCPKG_ROOT={vcpkg_root} ({vcpkg_source})")
    log(f"[env] cmake: {cmake} ({'.'.join(map(str, version))}, {cmake_source})")
    log(f"[env] ninja: {ninja} ({ninja_source})" if ninja else "[env] ninja: not found")
    return environment, cmake, vcpkg_root, version


def preset_name(args: argparse.Namespace, config: Config) -> str:
    """Preset from the command line, the config file, or the default."""
    if getattr(args, "preset", None):
        return args.preset
    return config.preset or "debug"


def job_count(args: argparse.Namespace, config: Config) -> int | None:
    if getattr(args, "jobs", None):
        return args.jobs
    return config.jobs


# ---------------------------------------------------------------------------
# Process helpers
# ---------------------------------------------------------------------------

def running_builds() -> list[tuple[str, str]]:
    """Return (pid, command line) of build processes that target this project."""
    script = (
        "Get-CimInstance Win32_Process -Filter "
        "\"Name='vcpkg.exe' OR Name='cmake.exe' OR Name='ninja.exe'\" | "
        "ForEach-Object { \"$($_.ProcessId)`t$($_.CommandLine)\" }"
    )
    try:
        completed = subprocess.run(
            ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script],
            capture_output=True,
            text=True,
            encoding=locale_encoding(),
            errors="replace",
        )
    except OSError:
        return []

    project = str(SOURCE_DIR).lower().replace("/", "\\")
    vcpkg_root = str(DEFAULT_VCPKG_ROOT).lower().replace("/", "\\")
    found: list[tuple[str, str]] = []
    for line in completed.stdout.splitlines():
        pid, _, command = line.partition("\t")
        lowered = command.lower()
        if project in lowered.replace("/", "\\") or (
            "vcpkg" in lowered and vcpkg_root in lowered.replace("/", "\\")
        ):
            found.append((pid.strip(), command.strip()))
    return found


def run_streaming(command: list[str], env: dict[str, str], cwd: Path, dry_run: bool) -> int:
    log("+ " + " ".join(command))
    if dry_run:
        return 0

    process = subprocess.Popen(
        command,
        cwd=str(cwd),
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding=locale_encoding(),
        errors="replace",
        bufsize=1,
    )
    assert process.stdout is not None

    interruptions = 0
    while True:
        try:
            line = process.stdout.readline()
        except KeyboardInterrupt:
            interruptions += 1
            if interruptions == 1:
                warn("interrupting a CMake/vcpkg run can leave vcpkg_installed "
                     "half-configured and force a full rebuild of Qt; "
                     "press Ctrl+C again to abort anyway")
                continue
            log("[abort] terminating the build process tree")
            subprocess.run(
                ["taskkill", "/PID", str(process.pid), "/T", "/F"],
                capture_output=True,
            )
            process.wait()
            return 130
        if not line:
            break
        print(line, end="", flush=True)

    return process.wait()


def read_preset_names(kind: str) -> list[str]:
    """Return preset names of a section (configurePresets, workflowPresets, ...)."""
    try:
        data = json.loads(PRESETS_FILE.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        fail(f"cannot read {PRESETS_FILE}: {error}")
    return [preset["name"] for preset in data.get(kind, []) if not preset.get("hidden")]


# ---------------------------------------------------------------------------
# vcpkg / build state diagnostics
# ---------------------------------------------------------------------------

def vcpkg_state() -> dict[str, object]:
    status = VCPKG_INSTALLED / "vcpkg" / "status"
    packages: list[str] = []
    if status.is_file():
        packages = [
            line.split(":", 1)[1].strip()
            for line in status.read_text(encoding="utf-8", errors="replace").splitlines()
            if line.startswith("Package: ")
        ]

    info_dir = VCPKG_INSTALLED / "vcpkg" / "info"
    info_files = sorted(info_dir.glob("*.list"), key=lambda path: path.stat().st_mtime, reverse=True) \
        if info_dir.is_dir() else []

    cache_dir = Path(os.environ.get("LOCALAPPDATA", "")) / "vcpkg" / "archives"
    cache_files = list(cache_dir.rglob("*.zip")) if cache_dir.is_dir() else []
    cache_bytes = sum(path.stat().st_size for path in cache_files)

    return {
        "status_exists": status.is_file(),
        "package_count": len(packages),
        "has_qtbase": "qtbase" in packages,
        "qt_config": (VCPKG_INSTALLED / "x64-windows" / "share" / "Qt6" / "Qt6Config.cmake").is_file(),
        "newest_info": info_files[0].name if info_files else None,
        "cache_files": len(cache_files),
        "cache_gb": round(cache_bytes / 1024 ** 3, 1),
    }


def cache_summary(build_dir: Path) -> dict[str, str]:
    cache = build_dir / "CMakeCache.txt"
    values: dict[str, str] = {}
    if not cache.is_file():
        return values
    wanted = ("CMAKE_COMMAND", "CMAKE_CXX_COMPILER", "VCPKG_TARGET_TRIPLET", "VCPKG_INSTALLED_DIR")
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        for key in wanted:
            if line.startswith(f"{key}:"):
                values[key] = line.split("=", 1)[1]
    return values


def abi_cmake_versions(vcpkg_root: Path, triplet: str = "x64-windows") -> tuple[dict[str, int], str | None]:
    """CMake versions recorded in vcpkg port ABI files.

    Returns the number of ports per recorded version and the version used by the
    most recent port build. vcpkg hashes the version of the CMake it builds
    ports with into every port ABI, so updating CMake marks all packages as
    stale and rebuilds them - which is why Qt is recompiled after an upgrade.
    """
    versions: dict[str, int] = {}
    latest_version: str | None = None
    latest_mtime = -1.0
    buildtrees = vcpkg_root / "buildtrees"
    if not buildtrees.is_dir():
        return versions, latest_version
    for info in buildtrees.glob(f"*/{triplet}.vcpkg_abi_info.txt"):
        try:
            with info.open(encoding="utf-8", errors="replace") as handle:
                recorded = next(
                    (line.split(None, 1)[1].strip() for line in handle if line.startswith("cmake ")),
                    None,
                )
            mtime = info.stat().st_mtime
        except OSError:
            continue
        if recorded is None:
            continue
        versions[recorded] = versions.get(recorded, 0) + 1
        if mtime > latest_mtime:
            latest_mtime = mtime
            latest_version = recorded
    return versions, latest_version


def doctor(args: argparse.Namespace) -> int:
    config = load_config(args.config)
    log(f"source directory: {SOURCE_DIR}")
    log(f"presets: configure={read_preset_names('configurePresets')} "
        f"workflow={read_preset_names('workflowPresets')}")
    log(f"config file: {config.path}"
        f"{'' if config.exists else ' (not found; copy build.ini.example to pin tool paths)'}")

    environment = dict(os.environ)
    vs_install, vs_source = resolve_vs_install(args.vs_install, config)
    log(f"visual studio: {vs_install or 'not found'} ({vs_source})")
    if vs_install is not None and not args.no_devshell:
        environment = developer_environment(vs_install)

    ninja, ninja_source = resolve_ninja(config, environment, vs_install)
    prepend_ninja(environment, ninja)
    log(f"ninja: {ninja or 'not found'} ({ninja_source})")

    vcpkg_root, vcpkg_source = resolve_vcpkg_root(args.vcpkg_root, config)
    environment["VCPKG_ROOT"] = str(vcpkg_root)
    log(f"vcpkg root: {vcpkg_root} ({vcpkg_source})")

    log("cmake candidates:")
    for candidate, source in cmake_candidates(args.cmake, config):
        version = cmake_version(candidate, environment) if candidate.is_file() else None
        state = ".".join(map(str, version)) if version else "missing"
        log(f"  - {candidate}: {state} ({source})")

    cmake, version, cmake_source = resolve_cmake(args.cmake, environment, config)
    log(f"selected cmake: {cmake} ({'.'.join(map(str, version))}, {cmake_source})")

    abi_versions, abi_latest = abi_cmake_versions(vcpkg_root)
    if abi_versions:
        summary = ", ".join(
            f"{value} ({count} ports)" for value, count in sorted(abi_versions.items(), key=lambda item: -item[1])
        )
        log(f"cmake recorded in vcpkg port ABI: {summary}")
        log(f"latest port build used CMake {abi_latest}")
        program_files_cmake = Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "CMake" / "bin" / "cmake.exe"
        discovered = cmake_version(program_files_cmake, environment) if program_files_cmake.is_file() else None
        discovered_text = ".".join(map(str, discovered)) if discovered else None
        if discovered_text and discovered_text != abi_latest:
            warn(f"vcpkg now uses CMake {discovered_text} ({program_files_cmake}) while the dependencies "
                 f"were built with {abi_latest}: a CMake update forces a full rebuild of every port")

    cl_path = shutil.which("cl.exe", path=environment.get("PATH", ""))
    if cl_path:
        match = re.search(r"Version ([\d.]+)", command_output([cl_path], environment))
        log(f"compiler: {cl_path} {match.group(1) if match else ''}".rstrip())
    else:
        warn("cl.exe is not on PATH; run the script without --no-devshell")

    state = vcpkg_state()
    log(f"vcpkg_installed: {VCPKG_INSTALLED}")
    log(f"  packages in status: {state['package_count']} (qtbase: {state['has_qtbase']})")
    log(f"  Qt6Config.cmake present: {state['qt_config']}")
    if state["newest_info"]:
        log(f"  newest installed package: {state['newest_info']}")
    log(f"binary cache: {state['cache_files']} archives, {state['cache_gb']} GB")

    if state["status_exists"] and not state["qt_config"]:
        warn("Qt is not installed in vcpkg_installed yet: the next build compiles "
             "qtbase from source, which takes tens of minutes. Let it finish once.")

    log(f"build defaults: preset={config.preset or 'debug'} jobs={config.jobs or 'unlimited'}")

    for build_dir in BUILD_DIRS.values():
        recorded = cache_summary(build_dir)
        if not recorded:
            log(f"{build_dir.name}: not configured yet")
            continue
        log(f"{build_dir.name}:")
        for key, value in recorded.items():
            log(f"  {key}={value}")
        recorded_cmake = recorded.get("CMAKE_COMMAND")
        if recorded_cmake and Path(recorded_cmake) != cmake:
            warn(f"{build_dir.name} was configured with {recorded_cmake}; "
                 "keeping a single CMake avoids vcpkg reinstalling every port "
                 "(run clean and configure again to switch deliberately)")

    active = running_builds()
    if active:
        warn(f"{len(active)} build process(es) currently running for this project")
        for pid, command in active[:5]:
            log(f"  pid {pid}: {command[:160]}")

    log("doctor finished")
    return 0


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def check_concurrency(args: argparse.Namespace) -> None:
    if args.dry_run:
        return
    active = running_builds()
    if active and not args.force:
        details = "\n".join(f"  pid {pid}: {command[:160]}" for pid, command in active[:5])
        fail("another build is already running for this project:\n"
             f"{details}\nvcpkg installs hold a lock and concurrent runs stall. "
             "Wait for it to finish or pass --force.")


def command_build(args: argparse.Namespace) -> int:
    check_concurrency(args)
    config = load_config(args.config)
    environment, cmake, _, _ = prepare_environment(args)
    jobs = job_count(args, config)
    if jobs:
        environment["CMAKE_BUILD_PARALLEL_LEVEL"] = str(jobs)
        log(f"[env] jobs: {jobs}")

    preset = preset_name(args, config)
    exit_code = run_streaming(
        [str(cmake), "--workflow", "--preset", preset],
        environment,
        SOURCE_DIR,
        args.dry_run,
    )
    if exit_code == 0 and getattr(args, "run", False) and not args.dry_run:
        return launch_application(preset)
    return exit_code


def command_configure(args: argparse.Namespace) -> int:
    check_concurrency(args)
    config = load_config(args.config)
    environment, cmake, _, _ = prepare_environment(args)
    return run_streaming(
        [str(cmake), "--preset", preset_name(args, config)],
        environment,
        SOURCE_DIR,
        args.dry_run,
    )


def command_clean(args: argparse.Namespace) -> int:
    config = load_config(args.config)
    build_dir = BUILD_DIRS[preset_name(args, config)]
    if not build_dir.name.startswith("build_"):
        fail(f"refusing to remove {build_dir}")
    if not build_dir.exists():
        log(f"{build_dir} does not exist, nothing to do")
        return 0
    if args.dry_run:
        log(f"would remove {build_dir}")
        return 0
    shutil.rmtree(build_dir)
    log(f"removed {build_dir}")
    log("vcpkg_installed was preserved on purpose: it keeps Qt and the other "
        "dependencies from being rebuilt")
    return 0


def launch_application(preset: str) -> int:
    build_preset = "release" if preset == "deploy" else preset
    candidates = [
        BUILD_DIRS[build_preset] / "dist" / "DBConnect.exe",
        BUILD_DIRS[build_preset] / "DBConnect.exe",
    ]
    for candidate in candidates:
        if candidate.is_file():
            log(f"launching {candidate}")
            subprocess.Popen([str(candidate)], cwd=str(candidate.parent))
            return 0
    warn("DBConnect.exe was not found; run the deploy workflow for the Release build")
    return 0


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def common_options_parser() -> argparse.ArgumentParser:
    """Options shared by the top-level command line and every subcommand."""
    parser = argparse.ArgumentParser(add_help=False)
    toolchain = parser.add_argument_group("toolchain options")
    toolchain.add_argument("--config", metavar="PATH",
                           help=f"build config file (default: {CONFIG_FILE})")
    toolchain.add_argument("--vs-install", metavar="PATH",
                           help="Visual Studio installation directory (contains Common7\\Tools)")
    toolchain.add_argument("--cmake", metavar="PATH", help="CMake executable (3.25 or newer)")
    toolchain.add_argument("--vcpkg-root", metavar="PATH",
                           help="standalone vcpkg checkout (default: %%VCPKG_ROOT%% or C:\\vcpkg)")
    toolchain.add_argument("--ninja", metavar="PATH", help="ninja.exe to use")
    toolchain.add_argument("--no-devshell", action="store_true",
                           help="do not import the Visual Studio environment; use the current one")
    run = parser.add_argument_group("run options")
    run.add_argument("--dry-run", action="store_true", help="print the command without running it")
    run.add_argument("--force", action="store_true", help="continue even if another build is running")
    return parser


def build_parser() -> argparse.ArgumentParser:
    common = common_options_parser()
    parser = argparse.ArgumentParser(
        description=(
            "Configure and build DBConnect with the shared CMake presets.\n"
            "Without a command the script runs 'build' with the Debug preset."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
        parents=[common],
        epilog=(
            "examples:\n"
            "  build.py doctor\n"
            "  build.py\n"
            "  build.py build --preset release\n"
            "  build.py deploy --run\n"
            "  build.py clean --preset debug\n"
            "  build.py build --cmake \"C:\\Program Files\\CMake\\bin\\cmake.exe\" --dry-run\n"
            f"\nTool paths can also be pinned in {CONFIG_FILE} (see build.ini.example).\n"
            "Precedence: command line, config file, environment, auto-detection.\n"
            "Run 'build.py <command> --help' for command-specific options."
        ),
    )
    subparsers = parser.add_subparsers(dest="command", metavar="{doctor,build,configure,deploy,clean}")

    doctor_parser = subparsers.add_parser(
        "doctor", help="show toolchain, config and vcpkg state", parents=[common],
        description="Print the resolved toolchain, the config file and the vcpkg state.")
    doctor_parser.set_defaults(handler=doctor)

    build_parser_ = subparsers.add_parser(
        "build", help="configure and build (cmake --workflow --preset)", parents=[common])
    build_parser_.add_argument("--preset", choices=["debug", "release"],
                               help="preset to use (default: from config, else debug)")
    build_parser_.add_argument("--jobs", type=int, metavar="N",
                               help="parallel build jobs (default: from config)")
    build_parser_.add_argument("--run", action="store_true", help="start the built executable")
    build_parser_.set_defaults(handler=command_build)

    configure_parser = subparsers.add_parser(
        "configure", help="configure only (cmake --preset)", parents=[common])
    configure_parser.add_argument("--preset", choices=["debug", "release"],
                                  help="preset to use (default: from config, else debug)")
    configure_parser.set_defaults(handler=command_configure)

    deploy_parser = subparsers.add_parser(
        "deploy", help="configure Release and run the deploy target", parents=[common],
        description="Configure the Release preset and package build_release/dist/.")
    deploy_parser.add_argument("--jobs", type=int, metavar="N", help="parallel build jobs")
    deploy_parser.add_argument("--run", action="store_true", help="start the deployed executable")
    deploy_parser.set_defaults(handler=command_build, preset="deploy")

    clean_parser = subparsers.add_parser(
        "clean", help="remove a build directory (keeps vcpkg_installed)", parents=[common],
        description="Remove build_debug or build_release. vcpkg_installed is always kept.")
    clean_parser.add_argument("--preset", choices=["debug", "release"],
                              help="build directory to remove (default: from config, else debug)")
    clean_parser.set_defaults(handler=command_clean)

    return parser


def main(argv: list[str] | None = None) -> int:
    arguments_list = list(sys.argv[1:] if argv is None else argv)
    commands = {"doctor", "build", "configure", "deploy", "clean"}
    if not arguments_list or (
        arguments_list[0] not in commands and arguments_list[0] not in ("-h", "--help")
    ):
        arguments_list = ["build", *arguments_list]

    parser = build_parser()
    arguments = parser.parse_args(arguments_list)
    try:
        return arguments.handler(arguments)
    except ScriptError as error:
        print(f"[error] {error}", file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    sys.exit(main())
