# Blitter core library

`blitter` is the static library that owns the persistent data model and transport-independent query execution. The other projects either adapt requests to it, prepare files for it, or exercise it directly.

## Public surface

[`blitter.h`](blitter.h) defines the main state and contracts:

- `SBlitter` owns configured databases, the current query, parsed configuration, database folder, expansion-key storage, expression reader, and file load cache.
- `SBlitterQuery` carries the database, command, optional expression path, request record, range, expansion chain, and detail record identity.
- `SDatabase` owns table metadata, block size, resident JSON blocks, block identities and offsets, access times, dirty bits, storage flags, encryption key, and discovered on-disk blocks.
- `loadConfig()` and `configDatabases()` turn the JSON configuration into table state.
- `requestProcess()` adapts an HTTP request into an `SBlitterQuery`.
- `queryProcess()` executes that query without depending on HTTP or CGI.
- `blitterFlush()` persists dirty resident blocks.

[`blitter_process.h`](blitter_process.h) exposes the lower-level record loading, single-record lookup, and range collection used by the query layer. [`blitter_output.h`](blitter_output.h) exposes the model-output helper.

## Implementation layout

| File | Responsibility |
| --- | --- |
| [`blitter_init.cpp`](blitter_init.cpp) | File naming, configuration loading, table description parsing, block discovery, on-demand block loading, cache replacement, and HTTP-request adaptation. |
| [`bitter_query_read.cpp`](bitter_query_read.cpp) | Absolute-record lookup and range-to-block mapping. The retained filename omits the `l` in `blitter`. |
| [`blitter_process.cpp`](blitter_process.cpp) | Dirty flushing, range/detail reads, relationship expansion, append logic, new-block creation, and command dispatch. |
| [`blitter_output_json.cpp`](blitter_output_json.cpp) | Builds a default JSON object from configured field types for the `model` command. |
| [`blitter_output_html.cpp`](blitter_output_html.cpp) | Entirely commented historical form-generation work; it is not active behavior. |
| [`blitter.json`](blitter.json) | Historical sample configuration and schema evidence. |
| [`refresh.bat`](refresh.bat) | Historical post-build staging helper. |

## Data and cache model

With `BlockSize == 0`, a table is loaded as one JSON array file. With a positive block size, records are addressed by `absolute index / block size`, and numbered files are loaded on demand. Resident slots retain the on-disk block identity and absolute offset separately from their in-memory slot index.

The cache is bounded to 1024 resident blocks per table in this source. When full, loading or creating another block selects the least-recent access time; dirty contents are written before the slot is reused. `blitterFlush()` walks every database and writes dirty blocks explicitly.

Range reads discover only blocks known to exist on disk, but preserve the requested absolute layout by emitting `{}` entries for missing blocks or records. That behavior protects stable record positions across sparse storage.

## Query behavior

`queryProcess()` dispatches these command strings:

| Command | Retained behavior |
| --- | --- |
| `get` | Returns one detail record or a range. Optional expansion recursively replaces configured integer references with records from bound tables. |
| `model` | Returns a default JSON object derived from configured fields. |
| `push_back` | Appends a parsed JSON value to the last block or creates a new block and returns its absolute record identity. |
| `form` | Recognized but returns success without producing form output. |
| `pop_back` | Explicit stub returning failure. |
| `replace` | Explicit stub returning failure. |

The expansion implementation accepts integer references and arrays of integer references. It finds the destination by table name or configured binding, then recursively processes the next expansion key.

## Configuration boundary

The sample configuration describes local tables, optional block sizes, deflation, encryption keys, bindings, public flags, and field metadata. A remote flag exists, but remote tables are skipped rather than loaded. `file_blocks` appears in the sample but is not established here as an active cache-limit setting; the compiled resident limit comes from `MAX_TABLE_BLOCKS_IN_MEMORY`.

## Known limits

- The source is historical and has not been built or run during this documentation pass.
- `pop_back`, `replace`, remote storage, and active HTML form output are unfinished.
- The adapter parses an expression path, but the retained tree is not sufficient evidence for a stable public query-language contract.
- Some error text and comments are stale or malformed. They should not be treated as behavioral specifications.

See the [solution overview](../README.md) for project relationships and the evidence boundary.
