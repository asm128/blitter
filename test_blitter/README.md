# `test_blitter` exploratory driver

`test_blitter` calls the Blitter library directly to explore queries, relationship expansion, insertion behavior, and insertion timing. Despite its name, the retained active program is not an isolated or assertion-based automated test suite.

## Components

[`test_blitter.cpp`](test_blitter.cpp) contains:

- `testQuery()`, which fills an `SBlitterQuery` and invokes `queryProcess()` for reads or model-style operations.
- `testPush()`, which parses a supplied JSON value into the query's `RecordReader` and invokes `queryProcess()` for writes.
- A large set of commented examples covering range reads, detail reads, and multi-hop expansions such as publisher-to-company relationships.
- Active timed insertion loops against configured `company`, `user`, and `publisher` tables.

## Mutation warning

The active path sets `iterations` to 1,000,000 and performs repeated `push_back` operations. It loads `./blitter.json`, can create or modify database blocks, and has its final `blitterFlush()` call commented out. Do not treat this executable as a harmless test discovery command or run it against valued data.

## Relationship to the system

The driver bypasses CGI and constructs the same `SBlitterQuery` consumed by the [`blitter` core](../blitter/README.md). It therefore demonstrates the transport-independent query boundary used by [`blit`](../blit/README.md), but it does not establish production correctness or a stable API contract.

## Status

The project was inspected statically only. No benchmark, query, build, or runtime validation was performed. See the [solution overview](../README.md) for the wider evidence boundary.
