#!/usr/bin/env python3

import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "benchmark" / "main.cc"
BINARY = ROOT / "build" / "benchmark_runner"
DATA_DIR = ROOT / "data"


def build():
    BINARY.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([
        "g++", "-std=c++20", "-O2",
        "-I", str(ROOT),
        "-I", str(ROOT / "benchmark"),
        "-I", str(ROOT / "benchmark" / "third_party"),
        "-I", str(ROOT / "caches"),
        str(SOURCE), "-o", str(BINARY),
    ], cwd=ROOT, check=True)
    print(f"Built: {BINARY}")


def run(config, log, data_dir):
    if not BINARY.is_file():
        raise SystemExit("Executable not found. First run: python3 bench.py build")

    config = config.resolve()
    log = log.resolve()
    data_dir = data_dir.resolve()
    if not config.is_file():
        raise SystemExit(f"Config not found: {config}")

    if not data_dir.is_dir():
        raise SystemExit(f"Test directory not found: {data_dir}")

    tests = sorted(p for p in data_dir.glob("*.json") if p.resolve() != config)
    if not tests:
        raise SystemExit(f"No JSON tests found in {data_dir}")

    log.parent.mkdir(parents=True, exist_ok=True)
    failed = 0

    with tempfile.TemporaryDirectory() as tmp, log.open("w") as output:
        for number, test in enumerate(tests, start=1):
            print(f"[{number}/{len(tests)}] {test.name}", flush=True)
            test_log = Path(tmp) / f"{number}.log"

            result = subprocess.run([
                str(BINARY), "-c", str(config),
                "-d", str(test), "-l", str(test_log),
            ], cwd=ROOT)

            output.write(f"\n===== {test.name} =====\n")
            if test_log.is_file():
                with test_log.open("r") as src:
                    shutil.copyfileobj(src, output)
                output.write("\n")
            if result.returncode != 0:
                failed += 1
                output.write(f"[FAILED: exit code {result.returncode}]\n")

    print(f"Finished: {len(tests)} tests, {failed} failed. Log: {log}")
    if failed:
        raise SystemExit(1)


def main():
    parser = argparse.ArgumentParser(description="Build and run cache benchmarks")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("build", help="Compile benchmark/main.cc")

    run_parser = commands.add_parser("run", help="Run all JSON tests in data/")
    run_parser.add_argument("-c", "--config", type=Path, required=True)
    run_parser.add_argument("-l", "--log", type=Path, required=True)
    run_parser.add_argument("-d", "--data-dir", type=Path, required=True,
                            help="Directory containing JSON tests")

    args = parser.parse_args()
    if args.command == "build":
        build()
    else:
        run(args.config, args.log, args.data_dir)


if __name__ == "__main__":
    main()