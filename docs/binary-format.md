# Binary parsed-state format

`ArgParser::save(path)` persists the last successfully parsed option state;
`read(path)` restores that state into an already registered parser. It does not
serialize registration descriptions, conversion functions, callbacks, variable
addresses, defaults, or executable state. The file's argument identities must
match the parser's registered canonical flags (long flag when present,
otherwise short flag). The number and identities of registrations must match
exactly.

All integers are unsigned little-endian. Strings are byte sequences without a
terminating NUL. The file layout is:

| Field | Size | Description |
|---|---:|---|
| Magic | 4 bytes | ASCII `ARGP` |
| Version | 2 bytes | Currently `1` |
| Record count | 4 bytes | Number of registered arguments |
| Short flag length | 2 bytes | Byte length of short flag |
| Short flag | variable | May be empty |
| Long flag length | 2 bytes | Byte length of long flag |
| Long flag | variable | May be empty |
| Supplied | 1 byte | `0` or `1` |
| Value count | 4 bytes | Number of parsed values |
| Value records | variable | Repeated length (4 bytes) and string bytes |
| CRC-32 | 4 bytes | CRC-32/ISO-HDLC over all preceding bytes |

Records are written in registration order. CRC-32 uses reflected polynomial
`0xEDB88320`, initial value `0xFFFFFFFF`, and final XOR `0xFFFFFFFF`.

The reader validates the magic, version, checksum, record structure, duplicate
identities, registration match, and value counts before changing parser state.
The current maximum file size is 16 MiB. Invalid or incompatible files raise
`invalid_binary_format`; file access errors raise `file_read_error` or
`file_write_error`. Restoring state does not invoke callbacks or assign bound
variables. Values remain available through `values`, `get`, and `getAll`.
