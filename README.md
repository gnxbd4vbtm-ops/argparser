# ArgParser

ArgParser is a modular C++20 command-line argument parsing library.
It provides typed arguments, callback and variable targets, generated help, and
versioned binary persistence of parsed values.

## Installation

Use the CMake target `ArgParser::ArgParser` and include
`<parser/ArgParser.hpp>`. The library's compiled implementation must be linked
into your application.

## Documentation

- [Installation, usage, API, and design](docs/guide.md)
- [Binary state file format](docs/binary-format.md)
