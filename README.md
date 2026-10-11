# Blitter

Blitter is a historical C++ persistent JSON record and query system. Its useful architectural boundary is still clear in the retained source: transports produce an `SBlitterQuery`, while the core library owns query execution, table configuration, record identity, block loading, caching, relationship expansion, and persistence.

This tree is documentation and design evidence, not a currently supported database product. The source contains unfinished commands, dormant output code, historical dependency paths, and known inconsistencies. These READMEs describe what the checked-in source establishes; they do not claim that the solution currently builds or runs.

## Solution map

| Project | Type | Responsibility |
| --- | --- | --- |
| [`blitter`](blitter/README.md) | Static library | Core configuration, query, record, block-cache, relationship-expansion, and output logic. |
| [`blit`](blit/README.md) | CGI application | Adapts an HTTP/CGI request into an `SBlitterQuery` and returns the core's JSON output. |
| [`season`](season/README.md) | Console application | Splits a JSON array into block files compatible with a block-sized Blitter table. |
| [`reason`](reason/README.md) | Console application | Writes a JSON array as one optional compressed/encrypted table file. |
| [`test_blitter`](test_blitter/README.md) | Console application | Deterministic tests for the transport-independent request, query, model, naming, and resident-record contracts. |

The solution also references external `llc` and `zlibvc` projects outside this repository. The retained Visual Studio projects use C++20 and historical include, library, output, and post-build paths tied to the surrounding GPK workspace.

## System relationship

```text
JSON input ── season/reason ──> table files or block directories
                                      │
HTTP/CGI ── blit ──> SBlitterQuery ──> blitter core ──> JSON result
                                      │
direct calls ── test_blitter ─────────┘
```

`blit` is only a request adapter. The transport-independent seam is `queryProcess()`, which also lets the test driver call the same database behavior directly. `season` and `reason` are companion data writers; neither is part of request handling.

## Confirmed core behavior

- Tables are described by [`blitter/blitter.json`](blitter/blitter.json): database path, table name, block size, storage flags, bindings, and field metadata.
- A table can be represented by one JSON array file or by numbered block files. Local block directories are discovered during configuration, while blocks are loaded on demand.
- Absolute record positions map to a block number and an index within that block. Range output preserves gaps with empty JSON objects instead of compacting later record identities.
- The resident block collection is bounded by `MAX_TABLE_BLOCKS_IN_MEMORY` (1024 in this source). Replacement selects an older resident slot and includes dirty-block writeback machinery.
- Optional deflation and encryption are routed through GPK's secure file helpers.
- Bindings and dot-separated expansion keys allow a record field containing an integer or integer array to be expanded through another configured table.
- Implemented query branches include `get`, `model`, and `push_back`. `form` is a no-output placeholder; `pop_back` and `replace` explicitly return “not implemented.”
- The configuration enum represents remote storage, but remote databases are skipped during local configuration. The remote path is not implemented here.

## Request shape in the retained adapter

The CGI adapter derives the database, command, and optional numeric detail from path segments. Query-string keys include `offset`, `limit` (or `count`), and dot-separated `expand`. It parses a dotted suffix on the database segment as an expression path. This describes the source-level adapter, not a stable or currently deployed HTTP contract.

## Configuration and generated data

[`blitter/blitter.json`](blitter/blitter.json) is a sample configuration, not a neutral template: it names specific historical tables and contains an embedded sample key. Treat it as evidence of the schema rather than production configuration.

[`blitter/refresh.bat`](blitter/refresh.bat) is a historical staging helper that creates output folders, copies the configuration into old build layouts, and copies selected executables. It is not a general setup script.

## Evidence boundary

These notes were recovered by inspecting the solution, every contained project file, all project entry points, the core headers and implementation, the sample configuration, and the existing [architecture and expression lineage](../../AGENT_RECOLLECTIONS/CommonGround/task_records/blitter-history/LINEAGE.md). The lineage explains why Blitter matters as a precedent, but current source is the authority for the behavior described here. No repair, build, test execution, or runtime validation was performed.
