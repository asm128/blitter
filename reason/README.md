# `reason` single-table writer

`reason` is a console companion that rewrites one JSON array into the single-file table format used when a Blitter table has no block size.

## Retained behavior

[`main.cpp`](main.cpp) reads the source file, requires an array root, serializes that root back to JSON, derives a database name from the filename, chooses the core library's table filename, and writes the result through GPK's secure file helper.

The arguments used by the code are:

```text
reason <source-json> [deflated-output] [encryption-key]
```

The deflation flag is true when its first character is not `0`. A non-empty third argument is passed as the encryption key.

## Relationship to Blitter

`reason` and [`season`](../season/README.md) are complementary data writers:

- `reason` produces one table file for `BlockSize == 0`.
- `season` produces a directory of numbered blocks for a positive block size.

Neither tool executes queries. Both depend on the [`blitter` core](../blitter/README.md) for storage naming and on external GPK/LLC helpers for JSON and file encoding.

## Historical caveats

Comments and the usage string still describe an older “split” interface and mention a block-size position that the implementation does not read. The encryption-status logging condition is also inverted. These inconsistencies are retained source defects, so this README documents the executed argument handling rather than the stale text.

The project was not built or executed during this documentation pass. See the [solution overview](../README.md).
