#!/usr/bin/env python3
import argparse
import math
import struct


def parse_header(file):
    header = []
    vertex_count = None
    properties = []
    in_vertex = False

    while True:
        raw = file.readline()
        if not raw:
            raise RuntimeError("Unexpected EOF before end_header")

        line = raw.decode("ascii").rstrip("\n")
        header.append(line)

        if line.startswith("format ") and line != "format binary_little_endian 1.0":
            raise RuntimeError("Only binary_little_endian PLY files are supported")

        if line.startswith("element "):
            parts = line.split()
            in_vertex = parts[1] == "vertex"
            if in_vertex:
                vertex_count = int(parts[2])
            continue

        if in_vertex and line.startswith("property "):
            parts = line.split()
            if parts[1] != "float":
                raise RuntimeError(f"Unsupported vertex property type: {parts[1]}")
            properties.append(parts[2])
            continue

        if line == "end_header":
            break

    if vertex_count is None:
        raise RuntimeError("Missing vertex element")

    return header, vertex_count, properties


def rewrite_vertex_count(header, count):
    return [
        f"element vertex {count}" if line.startswith("element vertex ") else line
        for line in header
    ]


def property_index(properties, name):
    try:
        return properties.index(name)
    except ValueError as exc:
        raise RuntimeError(f"Missing required property: {name}") from exc


def sigmoid(x):
    return 1.0 / (1.0 + math.exp(-x))


def main():
    parser = argparse.ArgumentParser(
        description="Downsample a GraphDeCo-style 3DGS binary PLY by splat importance."
    )
    parser.add_argument("input")
    parser.add_argument("output")
    parser.add_argument("--count", type=int, default=50000)
    args = parser.parse_args()

    with open(args.input, "rb") as file:
        header, vertex_count, properties = parse_header(file)
        row_stride = len(properties) * 4
        rows = [
            file.read(row_stride)
            for _ in range(vertex_count)
        ]

    if any(len(row) != row_stride for row in rows):
        raise RuntimeError("Input PLY ended before all vertex rows were read")

    opacity_i = property_index(properties, "opacity")
    scale_i = [
        property_index(properties, "scale_0"),
        property_index(properties, "scale_1"),
        property_index(properties, "scale_2"),
    ]

    scored = []
    row_format = "<" + "f" * len(properties)
    for index, row in enumerate(rows):
        values = struct.unpack(row_format, row)
        opacity = sigmoid(values[opacity_i])
        max_scale = max(math.exp(values[i]) for i in scale_i)
        scored.append((opacity * max_scale, index))

    keep_count = min(args.count, vertex_count)
    keep = sorted(index for _, index in sorted(scored, reverse=True)[:keep_count])

    with open(args.output, "wb") as file:
        for line in rewrite_vertex_count(header, keep_count):
            file.write((line + "\n").encode("ascii"))
        for index in keep:
            file.write(rows[index])

    print(f"Wrote {keep_count} / {vertex_count} splats to {args.output}")


if __name__ == "__main__":
    main()
