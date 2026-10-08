# `season` block-table writer

`season` is a console companion that converts one JSON array into the numbered block-file layout consumed by a block-sized Blitter table.

## Retained behavior

[`main.cpp`](main.cpp) performs this sequence:

1. Read and parse the source file.
2. Require the JSON root to be an array.
3. Derive the database name from the source filename.
4. Create the table folder name from the database name and block size.
5. Split the array into JSON arrays of at most the requested block size.
6. Write each numbered block through GPK's secure file helper, with optional deflation and encryption.

The arguments used by the code are:

```text
season <source-json> [block-size] [deflated-output] [encryption-key]
```

The default block size is 1024. A deflation flag is true when its first character is not `0`.

## Relationship to Blitter

The writer uses the core library's `tableFolderName()` and `blockFileName()` helpers, so its output follows the same naming rules as on-demand block loading. It prepares data only; it does not load configuration or execute queries.

## Historical caveats

The executable's embedded usage text calls the fourth optional value “deflated input,” but the implementation uses that position as an encryption key. The source also prints the supplied key. Treat the code-derived argument list above as documentation of the retained implementation, not a recommendation to use it with sensitive material.

The project was not built or executed during this documentation pass. See the [solution overview](../README.md) and [`blitter` core](../blitter/README.md).
