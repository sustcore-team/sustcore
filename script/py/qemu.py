#!/usr/bin/env python3
"""Emit QEMU configuration from qemu.toml."""

from mk_emitter import MkEmitter


def emit_drive(value: dict, captures: dict[str, str]) -> list[str]:
    required = ("file", "type", "format")
    missing = [key for key in required if key not in value]
    if missing:
        arch = captures.get("arch", "unknown")
        raise ValueError(
            f"{arch}.drives entry is missing: {', '.join(missing)}"
        )

    drive_id = f"x{captures['index']}"
    return [
        f"-drive file={value['file']},if=none,format={value['format']},id={drive_id}",
        f"-device {value['type']},drive={drive_id}",
    ]


def emit_memory(value: dict) -> str:
    required = ("size", "maxmem")
    missing = [key for key in required if key not in value]
    if missing:
        raise ValueError(f"memory is missing: {', '.join(missing)}")
    return f"-m size={value['size']}m,maxmem={value['maxmem']}m"


def emit_rtc(value: dict) -> str:
    required = ("base", "clock")
    missing = [key for key in required if key not in value]
    if missing:
        raise ValueError(f"rtc is missing: {', '.join(missing)}")
    return f"-rtc base={value['base']},clock={value['clock']}"


def emit_qemu_log(value: dict, captures: dict[str, str]) -> list[str]:
    if not isinstance(value, dict):
        arch = captures.get("arch", "unknown")
        raise TypeError(f"{arch}.qemu_log must be a table")

    file_value = value.get("file")
    if file_value is None:
        file_value = ""
    if not isinstance(file_value, str):
        arch = captures.get("arch", "unknown")
        raise TypeError(f"{arch}.qemu_log.file must be a string")

    type_values = value.get("type", [])
    trace_values = value.get("trace", [])
    if not isinstance(type_values, list) or not all(
        isinstance(item, str) for item in type_values
    ):
        arch = captures.get("arch", "unknown")
        raise TypeError(f"{arch}.qemu_log.type must be an array of strings")
    if not isinstance(trace_values, list) or not all(
        isinstance(item, str) for item in trace_values
    ):
        arch = captures.get("arch", "unknown")
        raise TypeError(f"{arch}.qemu_log.trace must be an array of strings")

    fragments = []
    if file_value:
        fragments.append(f"-D {file_value}")

    debug_items = list(type_values)
    debug_items.extend(f"trace:{item}" for item in trace_values)
    if debug_items:
        fragments.append(f"-d {','.join(debug_items)}")

    return fragments


def emit(data: dict) -> str:
    emitter = MkEmitter(data)
    emitter.keymap("$arch.qemu", "$arch-qemu")
    emitter.collect("$arch.qemu", lambda _, __: [], "$arch")
    emitter.collect("$arch.qemu", lambda _, __: [], "$arch-attached")
    emitter.collect("$arch.name", lambda value, _: [f'-name "{value}"'], "$arch")
    emitter.collect("$arch.qemu_log", emit_qemu_log, "$arch")
    emitter.collect("$arch.memory", lambda value, _: [emit_memory(value)], "$arch")
    emitter.collect("$arch.rtc", lambda value, _: [emit_rtc(value)], "$arch")
    emitter.collect("$arch.drives", emit_drive, "$arch")
    emitter.collect("$arch.attached", lambda value, _: [value], "$arch-attached")
    emitter.emit_collection("$arch", "$arch-qemu-generated-args")
    emitter.emit_collection("$arch-attached", "$arch-qemu-attached-args")
    return emitter.render("qemu.toml")
