#!/usr/bin/env python3
"""Combine PlatformIO's factory firmware and LittleFS into one install image."""

import argparse
import csv
from pathlib import Path


def parse_number(value: str) -> int:
    return int(value.strip(), 0)


def find_filesystem_partition(partition_file: Path) -> tuple[int, int]:
    with partition_file.open(newline="", encoding="utf-8") as source:
        rows = csv.reader(line for line in source if not line.lstrip().startswith("#"))
        for row in rows:
            if len(row) < 5:
                continue
            name, partition_type, _subtype, offset, size = (field.strip() for field in row[:5])
            if partition_type == "data" and name.lower() in {"littlefs", "spiffs"}:
                return parse_number(offset), parse_number(size)
    raise ValueError(f"No LittleFS/SPIFFS data partition found in {partition_file}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--environment", required=True, help="PlatformIO environment name")
    parser.add_argument("--project-dir", type=Path, default=Path.cwd())
    parser.add_argument("--output-name", default="cerasmarter-complete.bin")
    args = parser.parse_args()

    project_dir = args.project_dir.resolve()
    build_dir = project_dir / ".pio" / "build" / args.environment
    factory_file = build_dir / "firmware.factory.bin"
    filesystem_file = build_dir / "littlefs.bin"
    partition_file = project_dir / "partitions_custom.csv"
    output_file = build_dir / args.output_name

    for required_file in (factory_file, filesystem_file, partition_file):
        if not required_file.is_file():
            raise FileNotFoundError(f"Required build input is missing: {required_file}")

    filesystem_offset, filesystem_size = find_filesystem_partition(partition_file)
    factory = factory_file.read_bytes()
    filesystem = filesystem_file.read_bytes()

    if len(factory) > filesystem_offset:
        raise ValueError("Factory firmware overlaps the configured filesystem partition")
    if len(filesystem) > filesystem_size:
        raise ValueError("LittleFS image exceeds the configured filesystem partition")

    with output_file.open("wb") as output:
        output.write(factory)
        output.write(b"\xff" * (filesystem_offset - len(factory)))
        output.write(filesystem)

    print(
        f"Created {output_file} ({output_file.stat().st_size} bytes): "
        f"factory firmware at 0x0, LittleFS at 0x{filesystem_offset:X}"
    )


if __name__ == "__main__":
    main()
