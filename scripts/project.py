#!/usr/bin/env python3
"""Run the everyday monorepo tasks from the repository root.

The script collects the routines that otherwise have to be started from
different directories: regenerating the API clients from the OpenAPI spec,
copying them into the code that actually runs, building or deploying
DBConnect, and running the core API tests.

Usage (from the repository root or anywhere else):

    python scripts/project.py doctor      # show the tools each task will use
    python scripts/project.py generate    # bundle spec -> clients -> live code
    python scripts/project.py generate --check   # verify the live code is fresh
    python scripts/project.py sync        # copy generated code only
    python scripts/project.py build       # build DBConnect (delegates to build.py)
    python scripts/project.py deploy      # Release build + dist/ package
    python scripts/project.py test        # core API pytest suite
    python scripts/project.py verify      # generate --check, then test
    python scripts/project.py setup       # install the tooling dependencies

``build`` and ``deploy`` forward every remaining argument to
``DBConnect/scripts/build.py`` (for example ``... build --preset release
--jobs 4``), so the CMake/vcpkg logic lives in exactly one place.

Tool paths can be pinned in ``scripts/project.ini`` (git-ignored, see
``project.ini.example``). Resolution order for every tool is: command line
option, then the config file, then the environment (``BOTC_PYTHON``,
``BOTC_NPM``), then auto-detection.

Only the Python standard library is used.
"""

from __future__ import annotations

import argparse
import configparser
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parent
OPENAPI_DIR = ROOT / "openapi"
OPENAPI_GENERATE = OPENAPI_DIR / "generate.py"
OPENAPI_SYNC = OPENAPI_DIR / "sync.py"
OPENAPI_REQUIREMENTS = OPENAPI_DIR / "requirements.txt"
OPENAPI_MODULES = OPENAPI_DIR / "node_modules"
OPENAPI_DIST = OPENAPI_DIR / "dist" / "openapi.json"
PYTHON_GEN = OPENAPI_DIR / "python_gen" / "models.py"
CPP_GEN = OPENAPI_DIR / "cpp_gen" / "client"
CORE_API_DIR = ROOT / "core" / "api"
DBCONNECT_BUILD = ROOT / "DBConnect" / "scripts" / "build.py"
CONFIG_FILE = SCRIPT_DIR / "project.ini"

GENERATOR_MODULE = "datamodel_code_generator"
GENERATOR_REQUIREMENT = "OpenAPI models (datamodel-code-generator)"
PYTEST_MODULE = "pytest"


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
    """Settings from ``project.ini`` plus the path they were read from."""

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

    def get_bool(self, section: str, option: str, default: bool = False) -> bool:
        value = self.get(section, option)
        if value is None:
            return default
        lowered = value.lower()
        if lowered in {"1", "true", "yes", "on"}:
            return True
        if lowered in {"0", "false", "no", "off"}:
            return False
        warn(f"config {self.path.name}: [{section}] {option} = {value!r} is not a boolean, ignoring it")
        return default


def load_config(explicit: str | None) -> Config:
    """Read the tool configuration; a missing local file is not an error."""
    if explicit:
        path = Path(explicit).expanduser()
        if not path.is_file():
            fail(f"config file not found: {path}")
    else:
        from_env = os.environ.get("BOTC_PROJECT_CONFIG")
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
# Tool discovery
# ---------------------------------------------------------------------------

def virtualenv_pythons() -> list[Path]:
    """Interpreter paths of the virtual environments used in this repository."""
    names = ("Scripts/python.exe", "bin/python") if os.name == "nt" else ("bin/python",)
    return [
        venv / name
        for venv in (
            ROOT / ".venv",
            CORE_API_DIR / ".venv",
            CORE_API_DIR / "venv",
        )
        for name in names
    ]


def resolve_python(explicit: str | None, config: Config | None = None) -> tuple[Path, str]:
    """Locate the interpreter used for tooling and tests."""
    candidates: list[tuple[Path, str]] = []
    if explicit:
        candidates.append((Path(explicit).expanduser(), "command line"))
    configured = config.get("tools", "python") if config else None
    if configured:
        candidates.append((Path(configured).expanduser(), f"config file {CONFIG_FILE.name}"))
    from_env = os.environ.get("BOTC_PYTHON")
    if from_env:
        candidates.append((Path(from_env).expanduser(), "environment BOTC_PYTHON"))
    candidates.extend((path, "virtual environment") for path in virtualenv_pythons())
    candidates.append((Path(sys.executable), "current interpreter"))

    tried: list[str] = []
    for path, source in candidates:
        if path.is_file():
            return path.resolve(), source
        tried.append(f"  - {path} ({source})")
    fail("no Python interpreter found:\n" + "\n".join(tried))


def resolve_npm(explicit: str | None, config: Config | None = None) -> tuple[Path, str]:
    """Locate npm; the OpenAPI pipeline is driven through its package scripts."""
    candidates: list[tuple[Path, str]] = []
    if explicit:
        candidates.append((Path(explicit).expanduser(), "command line"))
    configured = config.get("tools", "npm") if config else None
    if configured:
        candidates.append((Path(configured).expanduser(), f"config file {CONFIG_FILE.name}"))
    from_env = os.environ.get("BOTC_NPM")
    if from_env:
        candidates.append((Path(from_env).expanduser(), "environment BOTC_NPM"))
    on_path = shutil.which("npm")
    if on_path:
        candidates.append((Path(on_path), "PATH"))

    tried: list[str] = []
    for path, source in candidates:
        if path.is_file():
            return path.resolve(), source
        tried.append(f"  - {path} ({source})")
    fail("npm was not found; install Node.js or set [tools] npm in "
         f"{CONFIG_FILE.name}:\n" + "\n".join(tried))


def tool_environment() -> dict[str, str]:
    """Environment for child tools: UTF-8 output regardless of the console code page.

    npm, Node and the Python tools all write UTF-8 to a pipe, while the Windows
    console defaults to the OEM code page; without this the transcripts get
    mojibake for non-ASCII role names and messages.
    """
    environment = dict(os.environ)
    environment["PYTHONUTF8"] = "1"
    environment["PYTHONIOENCODING"] = "utf-8"
    environment.setdefault("NO_COLOR", "1")
    return environment


def command_output(command: list[str], cwd: Path | None = None) -> str:
    try:
        completed = subprocess.run(
            command,
            cwd=str(cwd) if cwd else None,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=tool_environment(),
        )
    except OSError:
        return ""
    return f"{completed.stdout}\n{completed.stderr}".strip()


def tool_version(command: list[str]) -> str | None:
    """First line of ``command --version`` output, or None if it cannot run."""
    output = command_output(command)
    return output.splitlines()[0].strip() if output else None


def has_module(python: Path, module: str) -> bool:
    try:
        completed = subprocess.run(
            [str(python), "-c", f"import {module}"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=tool_environment(),
        )
    except OSError:
        return False
    return completed.returncode == 0


def require_module(python: Path, module: str, description: str) -> None:
    if has_module(python, module):
        return
    fail(f"{description} is not installed for {python}.\n"
         f"Install the tooling dependencies with: python scripts/project.py setup")


def require_npm_dependencies() -> None:
    """The OpenAPI steps need openapi/node_modules (git-ignored)."""
    if OPENAPI_MODULES.is_dir():
        return
    fail(f"{OPENAPI_MODULES} is missing; npm packages are not installed.\n"
         "Run: python scripts/project.py setup")


# ---------------------------------------------------------------------------
# Process helpers
# ---------------------------------------------------------------------------

def run_streaming(command: list[str], cwd: Path, dry_run: bool = False) -> int:
    """Run a command, streaming its output, and return its exit code."""
    log("+ " + " ".join(str(part) for part in command))
    if dry_run:
        return 0

    process = subprocess.Popen(
        [str(part) for part in command],
        cwd=str(cwd),
        env=tool_environment(),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,
    )
    assert process.stdout is not None
    try:
        for line in process.stdout:
            print(line, end="", flush=True)
    except KeyboardInterrupt:
        warn("interrupting the running task")
        process.terminate()
        process.wait()
        return 130
    return process.wait()


def run_captured(command: list[str], cwd: Path) -> tuple[int, str]:
    """Run a short command and return its exit code with the combined output."""
    try:
        completed = subprocess.run(
            [str(part) for part in command],
            cwd=str(cwd),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            env=tool_environment(),
        )
    except OSError as error:
        return 1, str(error)
    output = f"{completed.stdout or ''}{completed.stderr or ''}".strip()
    return completed.returncode, output


# ---------------------------------------------------------------------------
# Diagnostics
# ---------------------------------------------------------------------------

def describe_versions(python: Path) -> None:
    log(f"python: {python}")
    log(f"  version: {tool_version([str(python), '--version']) or 'unknown'}")
    for module, label in ((GENERATOR_MODULE, "models generator"), (PYTEST_MODULE, "pytest")):
        state = "available" if has_module(python, module) else "MISSING"
        log(f"  {label}: {state}")

    node = shutil.which("node")
    log(f"node: {node or 'not found'}")
    if node:
        log(f"  version: {tool_version([node, '--version']) or 'unknown'}")
    java = shutil.which("java")
    log(f"java: {java or 'not found'} (required by openapi-generator)")
    if java:
        version = command_output([java, "-version"]).splitlines()
        log(f"  version: {version[0].strip() if version else 'unknown'}")


def openapi_state() -> None:
    """Report the npm packages and the generated artifacts of the OpenAPI pipeline."""
    log(f"openapi: {OPENAPI_DIR}")
    log(f"  npm packages installed: {OPENAPI_MODULES.is_dir()}")
    pinned = None
    openapitools = OPENAPI_DIR / "openapitools.json"
    if openapitools.is_file():
        try:
            pinned = json.loads(openapitools.read_text(encoding="utf-8")).get(
                "generator-cli", {}
            ).get("version")
        except (OSError, json.JSONDecodeError):
            pinned = None
    log(f"  openapi-generator version: {pinned or 'not pinned'}")
    for label, path in (
        ("bundled spec", OPENAPI_DIST),
        ("python models", PYTHON_GEN),
        ("cpp client", CPP_GEN),
    ):
        state = "present" if path.exists() else "missing"
        log(f"  {label}: {state} ({path.relative_to(ROOT)})")


def report_sync_state(python: Path) -> int:
    """Run a non-writing sync check and return its exit code."""
    if not OPENAPI_SYNC.is_file():
        warn(f"{OPENAPI_SYNC} is missing")
        return 1
    if not PYTHON_GEN.is_file() or not CPP_GEN.is_dir():
        log("  live code check: skipped (generated outputs are missing)")
        return 1
    code, output = run_captured([str(python), str(OPENAPI_SYNC), "--check"], ROOT)
    for line in output.splitlines():
        log(f"  {line}")
    return code


def command_doctor(args: argparse.Namespace) -> int:
    config = load_config(args.config)
    log(f"repository: {ROOT}")
    log(f"config file: {config.path}"
        f"{'' if config.exists else ' (not found; copy project.ini.example to pin tool paths)'}")

    python, source = resolve_python(args.python, config)
    describe_versions(python)
    log(f"  selected from: {source}")

    try:
        npm, npm_source = resolve_npm(args.npm, config)
        log(f"npm: {npm} ({tool_version([str(npm), '--version']) or 'unknown'}, {npm_source})")
    except ScriptError as error:
        warn(str(error))

    openapi_state()
    log("live code state:")
    if report_sync_state(python) != 0:
        log("  run: python scripts/project.py generate")

    if args.with_dbconnect:
        if not DBCONNECT_BUILD.is_file():
            warn(f"{DBCONNECT_BUILD} is missing")
        else:
            log("")
            log("--- DBConnect toolchain (DBConnect/scripts/build.py doctor) ---")
            return run_streaming([str(python), str(DBCONNECT_BUILD), "doctor"], ROOT)
    else:
        log(f"DBConnect toolchain: run 'python scripts/project.py doctor --with-dbconnect' "
            f"or '{DBCONNECT_BUILD.relative_to(ROOT)} doctor'")
    return 0


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def command_setup(args: argparse.Namespace) -> int:
    config = load_config(args.config)
    python, python_source = resolve_python(args.python, config)
    log(f"python: {python} ({python_source})")

    if not args.skip_npm:
        npm, npm_source = resolve_npm(args.npm, config)
        log(f"npm: {npm} ({npm_source})")
        code = run_streaming([str(npm), "install"], OPENAPI_DIR, args.dry_run)
        if code != 0:
            return code

    if not args.skip_python:
        if not OPENAPI_REQUIREMENTS.is_file():
            fail(f"{OPENAPI_REQUIREMENTS} is missing")
        code = run_streaming(
            [str(python), "-m", "pip", "install", "-r", str(OPENAPI_REQUIREMENTS)],
            ROOT,
            args.dry_run,
        )
        if code != 0:
            return code

    log("setup complete")
    return 0


def resolve_step(skip_flag: bool, with_flag: bool, config_skip: bool, name: str) -> bool:
    """Combine the CLI skip/enable flags with the config default for one step."""
    if skip_flag and with_flag:
        fail(f"{name} and its --with- counterpart cannot be combined")
    if skip_flag:
        return True
    if with_flag:
        return False
    return config_skip


def command_generate(args: argparse.Namespace) -> int:
    config = load_config(args.config)
    if args.check and args.dry_run:
        fail("--check and --dry-run cannot be combined: a check has to run the tools")
    python, python_source = resolve_python(args.python, config)

    skip_python = resolve_step(args.skip_python, args.with_python,
                               config.get_bool("generate", "skip_python"), "--skip-python")
    skip_cpp = resolve_step(args.skip_cpp, args.with_cpp,
                            config.get_bool("generate", "skip_cpp"), "--skip-cpp")
    if skip_python and skip_cpp:
        fail("--skip-python and --skip-cpp together leave nothing to generate")

    npm: Path | None = None
    needs_npm = not args.skip_bundle or not skip_cpp
    if needs_npm:
        npm, npm_source = resolve_npm(args.npm, config)
        require_npm_dependencies()
        log(f"npm: {npm} ({npm_source})")

    if not skip_python:
        require_module(python, GENERATOR_MODULE, GENERATOR_REQUIREMENT)
    log(f"python: {python} ({python_source})")

    steps: list[tuple[list[str], Path]] = []
    if not args.skip_bundle:
        assert npm is not None
        steps.append(([str(npm), "run", "bundle"], OPENAPI_DIR))
    if not skip_python:
        steps.append(([str(python), str(OPENAPI_GENERATE)], ROOT))
    if not skip_cpp:
        assert npm is not None
        steps.append(([str(npm), "run", "generate:cpp"], OPENAPI_DIR))

    sync_command = [str(python), str(OPENAPI_SYNC)]
    if args.check:
        sync_command.append("--check")
    elif args.dry_run:
        sync_command.append("--dry-run")
    steps.append((sync_command, ROOT))

    for command, cwd in steps:
        code = run_streaming(command, cwd, args.dry_run)
        if code != 0:
            return code

    if args.dry_run:
        log("dry run — nothing was written")
        return 0
    if args.check:
        log("generated code is up to date")
        return 0
    log("generation complete; review the changes with 'git status' and 'git diff'")
    return 0


def command_sync(args: argparse.Namespace) -> int:
    config = load_config(args.config)
    if args.check and args.dry_run:
        fail("--check and --dry-run cannot be combined: a check has to read the current files")
    python, source = resolve_python(args.python, config)
    log(f"python: {python} ({source})")

    command = [str(python), str(OPENAPI_SYNC)]
    if args.check:
        command.append("--check")
    elif args.dry_run:
        command.append("--dry-run")
    # A check run must execute sync.py to know the answer.
    return run_streaming(command, ROOT, args.dry_run)


def command_build(args: argparse.Namespace) -> int:
    config = load_config(args.config)
    python, source = resolve_python(args.python, config)
    log(f"python: {python} ({source})")
    if not DBCONNECT_BUILD.is_file():
        fail(f"{DBCONNECT_BUILD} is missing")
    return run_streaming([str(python), str(DBCONNECT_BUILD), "build", *args.forwarded], ROOT)


def command_deploy(args: argparse.Namespace) -> int:
    config = load_config(args.config)
    python, source = resolve_python(args.python, config)
    log(f"python: {python} ({source})")
    if not DBCONNECT_BUILD.is_file():
        fail(f"{DBCONNECT_BUILD} is missing")
    return run_streaming([str(python), str(DBCONNECT_BUILD), "deploy", *args.forwarded], ROOT)


def pytest_command(python: Path, args: argparse.Namespace) -> list[str]:
    command = [str(python), "-m", "pytest", "app/tests/", "-q" if args.quiet else "-v"]
    if args.filter:
        command.extend(["-k", args.filter])
    command.extend(args.passed)
    return command


def command_test(args: argparse.Namespace) -> int:
    config = load_config(args.config)
    python, source = resolve_python(args.python, config)
    log(f"python: {python} ({source})")
    require_module(python, PYTEST_MODULE, "pytest")
    if not CORE_API_DIR.is_dir():
        fail(f"{CORE_API_DIR} is missing")
    return run_streaming(pytest_command(python, args), CORE_API_DIR, args.dry_run)


def command_verify(args: argparse.Namespace) -> int:
    """Fail when the generated code is stale, then run the core API tests."""
    check_args = argparse.Namespace(
        config=args.config,
        python=args.python,
        npm=args.npm,
        dry_run=False,
        check=True,
        skip_bundle=False,
        skip_python=False,
        skip_cpp=False,
        with_python=True,
        with_cpp=True,
    )
    code = command_generate(check_args)
    if code != 0:
        return code

    test_args = argparse.Namespace(
        config=args.config,
        python=args.python,
        dry_run=False,
        quiet=False,
        filter=None,
        passed=[],
    )
    return command_test(test_args)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def common_options_parser() -> argparse.ArgumentParser:
    """Options shared by the top-level command line and every subcommand."""
    parser = argparse.ArgumentParser(add_help=False)
    tools = parser.add_argument_group("tool options")
    tools.add_argument("--config", metavar="PATH",
                       help=f"project config file (default: {CONFIG_FILE})")
    tools.add_argument("--python", metavar="PATH",
                       help="Python interpreter for tooling and tests "
                            "(default: .venv, then the current interpreter)")
    tools.add_argument("--npm", metavar="PATH", help="npm executable (default: from PATH)")
    return parser


def build_parser() -> argparse.ArgumentParser:
    common = common_options_parser()
    parser = argparse.ArgumentParser(
        description=(
            "Run the routine monorepo tasks (OpenAPI generation, DBConnect builds, tests).\n"
            "Without a command the script prints this help."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
        parents=[common],
        epilog=(
            "examples:\n"
            "  project.py doctor\n"
            "  project.py generate\n"
            "  project.py generate --check\n"
            "  project.py sync --dry-run\n"
            "  project.py build --preset release --jobs 4\n"
            "  project.py deploy --run\n"
            "  project.py test -k import\n"
            "  project.py verify\n"
            f"\nTool paths can also be pinned in {CONFIG_FILE} (see project.ini.example).\n"
            "Precedence: command line, config file, environment, auto-detection.\n"
            "Run 'project.py <command> --help' for command-specific options."
        ),
    )
    subparsers = parser.add_subparsers(
        dest="command",
        metavar="{doctor,setup,generate,sync,build,deploy,test,verify}",
    )

    doctor_parser = subparsers.add_parser(
        "doctor", help="show tools, npm state and whether the live code is fresh",
        parents=[common],
        description="Print the resolved tools and the state of the generated code.")
    doctor_parser.add_argument("--with-dbconnect", action="store_true",
                               help="also run DBConnect/scripts/build.py doctor")
    doctor_parser.set_defaults(handler=command_doctor)

    setup_parser = subparsers.add_parser(
        "setup", help="install npm packages and the Python generator",
        parents=[common],
        description="Install openapi/node_modules and openapi/requirements.txt.")
    setup_parser.add_argument("--skip-npm", action="store_true", help="do not run npm install")
    setup_parser.add_argument("--skip-python", action="store_true",
                              help="do not install the Python requirements")
    setup_parser.add_argument("--dry-run", action="store_true",
                              help="print the commands without running them")
    setup_parser.set_defaults(handler=command_setup)

    generate_parser = subparsers.add_parser(
        "generate", help="regenerate both clients and copy them into the live code",
        parents=[common],
        description=(
            "Run the OpenAPI pipeline: redocly bundle, Python models, C++ client, "
            "then copy the results into core/api/app/models.py and DBConnect/generated_api/."))
    generate_parser.add_argument("--skip-bundle", action="store_true",
                                 help="reuse dist/openapi.json instead of bundling")
    generate_parser.add_argument("--skip-python", action="store_true",
                                 help="do not regenerate the Python models")
    generate_parser.add_argument("--with-python", action="store_true",
                                 help="regenerate the Python models even if the config skips them")
    generate_parser.add_argument("--skip-cpp", action="store_true",
                                 help="do not regenerate the C++ client")
    generate_parser.add_argument("--with-cpp", action="store_true",
                                 help="regenerate the C++ client even if the config skips it")
    generate_parser.add_argument("--check", action="store_true",
                                 help="fail when the live code does not match the generated files")
    generate_parser.add_argument("--dry-run", action="store_true",
                                 help="print the commands without running them")
    generate_parser.set_defaults(handler=command_generate)

    sync_parser = subparsers.add_parser(
        "sync", help="copy the generated clients into the live code", parents=[common],
        description="Copy openapi/python_gen and openapi/cpp_gen into the code that runs.")
    sync_parser.add_argument("--check", action="store_true",
                             help="exit non-zero when the live code is stale (writes nothing)")
    sync_parser.add_argument("--dry-run", action="store_true",
                             help="list what would change without writing anything")
    sync_parser.set_defaults(handler=command_sync)

    build_parser_ = subparsers.add_parser(
        "build", help="build DBConnect (forwards options to DBConnect/scripts/build.py)",
        parents=[common],
        description=("Configure and build DBConnect. Every argument after 'build' is "
                     "forwarded to DBConnect/scripts/build.py, so 'project.py build --help' "
                     "shows the CMake/vcpkg options."))
    build_parser_.set_defaults(handler=command_build)

    deploy_parser = subparsers.add_parser(
        "deploy", help="Release build + dist/ package (forwards to build.py)",
        parents=[common],
        description=("Run the DBConnect deploy workflow. Every argument after 'deploy' is "
                     "forwarded to DBConnect/scripts/build.py."))
    deploy_parser.set_defaults(handler=command_deploy)

    test_parser = subparsers.add_parser(
        "test", help="run the core API test suite", parents=[common],
        description="Run 'python -m pytest app/tests/' inside core/api.")
    test_parser.add_argument("-k", "--filter", metavar="EXPR",
                             help="only run tests matching the expression")
    test_parser.add_argument("-q", "--quiet", action="store_true", help="less verbose output")
    test_parser.add_argument("--dry-run", action="store_true",
                             help="print the command without running it")
    test_parser.set_defaults(handler=command_test, passed=[])

    verify_parser = subparsers.add_parser(
        "verify", help="check the generated code is fresh, then run the tests",
        parents=[common],
        description="Run 'generate --check' and the core API tests, in that order.")
    verify_parser.set_defaults(handler=command_verify)

    return parser


FORWARDING_COMMANDS = {"build", "deploy"}
COMMANDS = {
    "doctor", "setup", "generate", "sync", "build", "deploy", "test", "verify",
}


def split_forwarded(arguments: list[str]) -> tuple[list[str], str | None, list[str]]:
    """Split ``[own options] <build|deploy> [forwarded options]``.

    ``argparse`` cannot forward unknown options to a subcommand reliably, and
    some names (``--config``, ``--dry-run``) mean different things to this
    script and to ``build.py``. Splitting the raw command line keeps the
    boundary obvious: everything after the verb belongs to ``build.py``.

    Only the first command verb is considered, so a value that happens to be
    called ``build`` (for example ``test -k build``) is left alone.
    """
    for index, token in enumerate(arguments):
        if token in FORWARDING_COMMANDS:
            return arguments[:index], token, arguments[index + 1:]
        if token in COMMANDS:
            return arguments, None, []
    return arguments, None, []


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    arguments_list = list(sys.argv[1:] if argv is None else argv)
    if not arguments_list:
        parser.print_help()
        return 0

    own_arguments, verb, forwarded = split_forwarded(arguments_list)
    if verb is not None:
        arguments_list = [*own_arguments, verb]

    arguments, unknown = parser.parse_known_args(arguments_list)
    if unknown:
        if getattr(arguments, "command", None) == "test":
            arguments.passed = [*getattr(arguments, "passed", []), *unknown]
        else:
            parser.error(f"unrecognized arguments: {' '.join(unknown)}")
    if not hasattr(arguments, "handler"):
        parser.print_help()
        return 0

    if verb is not None:
        arguments.forwarded = forwarded
    try:
        return arguments.handler(arguments)
    except ScriptError as error:
        print(f"[error] {error}", file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    sys.exit(main())
