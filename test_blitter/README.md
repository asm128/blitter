# `test_blitter` core test suite

`test_blitter` exercises the transport-independent Blitter core without loading a configuration file or touching persistent storage.

## Components

[`blitter_test.cpp`](blitter_test.cpp) verifies:

- table and block file names for the supported storage flags;
- request path, range and expansion parsing;
- model output both directly and through `queryProcess()`;
- resident-record lookup and absolute-to-block index mapping.

## Stress testing

The former million-iteration insertion driver was removed. It mixed exploratory examples, mutation and timing in the normal test executable without isolating failures or flushing the data it changed. Future stress tests belong in a separate executable with bounded inputs, deterministic seeds and explicit invariants.

## Relationship to the system

The suite bypasses CGI and exercises the same request and query boundaries consumed by the [`blitter` core](../blitter/README.md). The CGI adapter remains a separate transport concern.

## Status

The project is intended to build and run against the independent LLC repository checked out beside Blitter. See the [solution overview](../README.md) for the wider migration boundary.
