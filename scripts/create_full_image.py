#!/usr/bin/env python3
"""Combine ESP32 boot components, application and LittleFS into one install image."""

import argparse
import csv
from pathlib import Path

from platformio.platform.factory import PlatformFactory
from platformio.project.config import ProjectConfig


def parse_number(value: str) -> int:
    return int(value.strip(), 0)


def find_partition(partition_file: Path, partition_type: str, subtype: str) -> tuple[int, int]:
    with partition_file.open(newline="", encoding="utf-8") as source:
        rows = csv.reader(line for line in source if not line.lstrip().startswith("#"))
        for row in rows:
            if len(row) < 5:
                continue
            _name, row_type, row_subtype, offset, size = (field.strip() for field in row[:5])
            if row_type == partition_type and row_subtype == subtype:
                return parse_number(offset), parse_number(size)
    raise ValueError(f"No {partition_type}/{subtype} partition found in {partition_file}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--environment", required=True, help="PlatformIO environment name")
    parser.add_argument("--project-dir", type=Path, default=Path.cwd())
    parser.add_argument("--output-name", default="cerasmarter-complete.bin")
    args = parser.parse_args()

    project_dir = args.project_dir.resolve()
    build_dir = project_dir / ".pio" / "build" / args.environment
    bootloader_file = build_dir / "bootloader.bin"
    partition_binary_file = build_dir / "partitions.bin"
    firmware_file = build_dir / "firmware.bin"
    filesystem_file = build_dir / "littlefs.bin"
    partition_file = project_dir / "partitions_custom.csv"
    output_file = build_dir / args.output_name

    config = ProjectConfig(str(project_dir / "platformio.ini"))
    platform = PlatformFactory.new(config.get(f"env:{args.environment}", "platform"))
    framework_dir = Path(platform.get_package_dir("framework-arduinoespressif32"))
    ota_file = framework_dir / "tools" / "partitions" / "boot_app0.bin"
    application_offset, application_size = find_partition(partition_file, "app", "ota_0")
    ota_offset, ota_size = find_partition(partition_file, "data", "ota")
    filesystem_offset, filesystem_size = find_partition(partition_file, "data", "spiffs")

    # These boot offsets belong to the project's classic ESP32 board. Partition
    # data, application and filesystem offsets come from its partition table.
    inputs = [
        (0x1000, 0x7000, bootloader_file),
        (0x8000, 0x1000, partition_binary_file),
        (ota_offset, ota_size, ota_file),
        (application_offset, application_size, firmware_file),
        (filesystem_offset, filesystem_size, filesystem_file),
    ]
    segments = []
    previous_end = 0
    for offset, capacity, input_file in sorted(inputs):
        if not input_file.is_file():
            raise FileNotFoundError(f"Required build input is missing: {input_file}")
        data = input_file.read_bytes()
        if len(data) > capacity:
            raise ValueError(f"{input_file.name} exceeds its flash region")
        if offset < previous_end:
            raise ValueError(f"{input_file.name} overlaps another flash region")
        segments.append((offset, data))
        previous_end = offset + len(data)

    with output_file.open("wb") as output:
        for offset, data in segments:
            output.write(b"\xff" * (offset - output.tell()))
            output.write(data)

    print(
        f"Created {output_file} ({output_file.stat().st_size} bytes): "
        f"boot components and application at their configured offsets, LittleFS at 0x{filesystem_offset:X}"
    )


if __name__ == "__main__":
    main()
