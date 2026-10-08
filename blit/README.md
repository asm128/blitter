# `blit` CGI adapter

`blit` is the HTTP/CGI-facing executable for Blitter. Its single entry point in [`main.cpp`](main.cpp) converts GPK CGI runtime values into an `SHTTPAPIRequest`, translates that request into an `SBlitterQuery`, loads the Blitter configuration, executes the query, and returns JSON output.

## Request flow

1. `httpRequestInit()` reads the CGI/runtime values.
2. CGI execution emits an `application/json` content header.
3. `requestProcess()` extracts database, command, optional detail identity, range parameters, expansion keys, request body, and optional expression path.
4. `loadConfig()` loads `./blitter.json` and initializes the configured databases.
5. `queryProcess()` performs the database operation and appends its result to the CGI output.

This separation is intentional: `blit` owns transport adaptation, while the [`blitter` core](../blitter/README.md) owns storage and query behavior.

## Source-level request shape

- Path segment 1: database name.
- Path segment 2: command.
- Final segment, when present: numeric detail record identity.
- Query string: `offset`, `limit` or `count`, and dot-separated `expand`.
- Request body: parsed as the JSON record used by write commands.

These observations are not a promise of a deployed API. The checked-in source includes an unused `itwapi.json` name while actually loading `./blitter.json`, and no runtime deployment was inspected.

## Status

The project is a historical Visual Studio application that links the Blitter library plus external LLC and zlib projects. It was not built or executed during this documentation pass. Error responses and query behavior remain subject to the incomplete core commands described in the [solution overview](../README.md).
