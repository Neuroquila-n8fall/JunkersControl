#!/usr/bin/env python3
"""Send JSON temperature inputs to the ESP32 temperature simulator on COM3.

Examples:
  python scripts/temperature_console.py --json '{"ValveScaling":true,"DynamicAdaption":true,"OutsideTemperature":11.5,"ValveOpening":32}'
  python scripts/temperature_console.py --all
  python scripts/temperature_console.py --interactive

Requires pyserial (python -m pip install pyserial).
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from typing import Any

import serial

SCENARIOS: dict[str, dict[str, Any]] = {
    "hybrid_mild": {
        "ValveScaling": True,
        "DynamicAdaption": True,
        "OutsideTemperature": 11.5,
        "ValveOpening": 32,
    },
    "hybrid_cold": {
        "ValveScaling": True,
        "DynamicAdaption": True,
        "OutsideTemperature": 0,
        "ValveOpening": 32,
    },
    "hybrid_winter_30": {
        "ValveScaling": True, "DynamicAdaption": True,
        "OutsideTemperature": -10, "ValveOpening": 30,
        "WeatherSupportMaximum": 60,
    },
    "hybrid_winter_1": {
        "ValveScaling": True, "DynamicAdaption": True,
        "OutsideTemperature": -10, "ValveOpening": 1,
    },
    "hybrid_winter_full_weather": {
        "ValveScaling": True, "DynamicAdaption": True,
        "OutsideTemperature": -10, "ValveOpening": 30,
        "WeatherSupportMaximum": 100,
    },
    "hybrid_winter_no_weather": {
        "ValveScaling": True, "DynamicAdaption": True,
        "OutsideTemperature": -10, "ValveOpening": 30,
        "WeatherSupportMaximum": 0,
    },
    "forecast_cooling": {
        "ValveScaling": True, "DynamicAdaption": True,
        "OutsideTemperature": -10, "ValveOpening": 30,
        "ForecastEnabled": True, "ForecastCoefficient": 0.5,
        "ForecastTemperatureChange": -4,
    },
    "forecast_warming_deficit": {
        "ValveScaling": True, "DynamicAdaption": True,
        "OutsideTemperature": -10, "ValveOpening": 30,
        "ForecastEnabled": True, "ForecastCoefficient": 0.5,
        "ForecastTemperatureChange": 4, "ForecastRoomsSatisfied": False,
    },
    "forecast_warming_satisfied": {
        "ValveScaling": True, "DynamicAdaption": True,
        "OutsideTemperature": -10, "ValveOpening": 30,
        "ForecastEnabled": True, "ForecastCoefficient": 0.5,
        "ForecastTemperatureChange": 4, "ForecastRoomsSatisfied": True,
    },
    "forecast_disabled": {
        "ValveScaling": True, "DynamicAdaption": True,
        "OutsideTemperature": -10, "ValveOpening": 30,
        "ForecastEnabled": False, "ForecastCoefficient": 0.5,
        "ForecastTemperatureChange": -4,
    },
    "static_valve_32": {"ValveScaling": True, "ValveOpening": 32},
    "disabled": {"Enabled": False, "ForecastCorrection": 2},
    "failsafe_hold": {
        "HoldMinimumTemperature": True,
        "MinimumFeedTemperature": 35,
        "BasepointTemperature": 21,
        "EndpointTemperature": 21,
        "OutsideTemperatureValid": False,
        "ForecastCorrection": 2,
    },
}


def read_response(connection: serial.Serial) -> dict[str, Any]:
    deadline = time.monotonic() + 3.0
    while time.monotonic() < deadline:
        line = connection.readline()
        if not line:
            continue
        try:
            response = json.loads(line.decode("utf-8", errors="strict"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            print(f"Ignored non-JSON serial line: {line!r}", file=sys.stderr)
            continue
        if isinstance(response, dict) and response.get("ready") is True:
            continue
        if not isinstance(response, dict):
            raise RuntimeError(f"Expected a JSON object response, got {response!r}")
        return response
    raise TimeoutError("No JSON response received from simulator within 3 seconds")


def exchange(connection: serial.Serial, inputs: dict[str, Any]) -> dict[str, Any]:
    payload = json.dumps(inputs, separators=(",", ":"))
    if len(payload.encode("utf-8")) > 511:
        raise ValueError("JSON input exceeds simulator's 511-byte line limit")
    connection.write(payload.encode("utf-8") + b"\n")
    connection.flush()
    response = read_response(connection)
    print(json.dumps({"input": inputs, "response": response}, separators=(",", ":")))
    return response


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM3", help="serial port (default: COM3)")
    parser.add_argument("--baud", type=int, default=115200)
    choice = parser.add_mutually_exclusive_group()
    choice.add_argument("--json", help="one JSON object containing named FeedTemperatureInputs fields")
    choice.add_argument("--scenario", choices=sorted(SCENARIOS))
    choice.add_argument("--all", action="store_true", help="run every predefined scenario")
    choice.add_argument("--interactive", action="store_true", help="read JSON objects from stdin until EOF")
    args = parser.parse_args()

    try:
        connection = serial.Serial(args.port, args.baud, timeout=0.2, write_timeout=2.0)
    except serial.SerialException as error:
        parser.error(f"cannot open {args.port}: {error}")

    with connection:
        # Allow the ESP32 to reset after USB serial opens.
        time.sleep(2.0)
        connection.reset_input_buffer()
        if args.json is not None:
            try:
                inputs = json.loads(args.json)
            except json.JSONDecodeError as error:
                parser.error(f"--json is invalid JSON: {error}")
            if not isinstance(inputs, dict):
                parser.error("--json must be a JSON object")
            exchange(connection, inputs)
        elif args.scenario:
            exchange(connection, SCENARIOS[args.scenario])
        elif args.all:
            for inputs in SCENARIOS.values():
                exchange(connection, inputs)
        elif args.interactive:
            for line in sys.stdin:
                if line.strip():
                    inputs = json.loads(line)
                    if not isinstance(inputs, dict):
                        raise ValueError("each input line must be a JSON object")
                    exchange(connection, inputs)
        else:
            line = input("Input JSON object: ")
            inputs = json.loads(line)
            if not isinstance(inputs, dict):
                parser.error("input must be a JSON object")
            exchange(connection, inputs)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
