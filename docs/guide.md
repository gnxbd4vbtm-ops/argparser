# ArgParser guide

## Installation and integration

ArgParser requires C++20 and has no third-party dependencies. Add this repository
with `add_subdirectory` and link `ArgParser::ArgParser`, or install the exported
CMake interface target with:

```sh
cmake -S . -B build
cmake --build build
cmake --install build --prefix /some/prefix
```

For a direct include, add the repository's `include/` directory to the compiler
include path, include `<parser/ArgParser.hpp>`, and link the built ArgParser
library.

## Basic use

```cpp
#include <parser/ArgParser.hpp>

int main(int argc, char** argv)
{
    parser::ArgParser args;
    int timeout = 0;
    args.arg("-b", "--build", "Build the project", [] { /* build */ });
    args.addArg({.shortFlag = "-t", .longFlag = "--timeout",
                 .description = "Timeout in seconds", .required = true,
                 .target = &timeout});
    args.parse(argc, argv);
}
```

No-argument callables register flags. A callable accepting one typed value is
called with a converted value. A mutable pointer target binds a parsed value to
that variable. `bool` targets are flags: presence means `true`; an optional
explicit `true`, `false`, `1`, or `0` value is also accepted. A default-constructed
target accepts no values.

Long options accept `--name=value` and `--name value`; short options accept
`-n value`. Short-option bundling (such as `-abc`) is not supported. An option
with no value may appear alone; positional arguments are not supported.
`parse` returns `false` after displaying built-in help and `true` after a
successful parse, so applications can skip normal execution when help was
requested.

## Advanced registration and value types

`ArgumentConfig` is an aggregate. At least one of `shortFlag` and `longFlag` is
required. Short flags have the form `-x`; long flags have the form `--name`.
Every alias is unique across the parser.

Typed targets support booleans, integral and floating-point values, strings,
`std::filesystem::path`, vectors of supported values, and custom types. Define
`parser::Converter<T>` before registration to add a custom type:

```cpp
struct Port { int value; };

template <>
struct parser::Converter<Port>
{
    static Port convert(std::string_view text)
    {
        return {parser::convert<int>(text)};
    }
    static constexpr std::string_view type_name() noexcept { return "PORT"; }
};
```

The converter must expose `static T convert(std::string_view)` and
`static constexpr std::string_view type_name()`. The default converter also
supports stream-extractable custom types.

Use `minValues` and `maxValues` to configure the number accepted for an option.
`Target` inferred types default to one value, except booleans (zero or one) and
vector targets (one or more). An omitted maximum means unbounded. Values are
consumed until another flag or the configured maximum. Setting a scalar target's
minimum to zero makes its value optional: if omitted, the target is left
unchanged and its typed callback is not called. An explicit empty string is a
valid string value; numeric conversions reject it.

Repeated options are rejected by default. Set `allowRepeated = true` to accept
them; `values()` then returns values in encounter order. Callbacks run once per
occurrence. For a repeated scalar variable target, the last callback assignment
wins. A vector target receives the values for each occurrence.

Integral and floating-point conversions require the entire value to be valid.
Negative numeric tokens are treated as values when the target type is numeric.
The shell performs ordinary quote removal before invoking the program; the
parser does not interpret quote characters or run shell commands. `$NAME` and
`${NAME}` are expanded in option values. An unset variable raises `invalid_value`.
Command substitution is never evaluated.

## Errors and parser state

All parser exceptions derive from `parser::error`. Specific types include
`required_argument_missing`, `unknown_argument`, `missing_argument_value`,
`invalid_value`, `duplicate_flag`, `invalid_argument_definition`,
`file_read_error`, `file_write_error`, and `invalid_binary_format`.

On parsing errors, usage is written to standard error before the exception is
re-thrown. Call `setAutomaticUsage(false)` to disable this behavior. Parsing
validates all options and required flags before updating the stored parse state
or invoking callbacks. A validation failure therefore leaves the previous
successful state unchanged. Callback exceptions occur after state is committed.

`required` means that the option must be present in each parse. Empty input is
valid when there are no required options. Unknown flags and unexpected
positional values are errors.

## Help formatting

`usage()` returns generated usage text and `printHelp()` writes it to a stream.
The parser recognizes `-h` and `--help` without reserving those names in the
registration table. Help includes registered options, required/optional status,
types, configured default labels, and examples. Required options are marked
with `*` after their option label; optional options have no marker. Set the
application name and description with `setApplicationName` and
`setApplicationDescription`, and add examples with `addExample`. Set colors
with `setColorsEnabled`; presence of the `NO_COLOR` environment variable always
disables ANSI color.

`defaultValue` is display metadata only: it is shown in help but does not insert
a value, satisfy `required`, or initialize a target.

## Argument management and retrieval

- `hasArg` / `exists` checks a registered flag.
- `wasProvided` checks whether a flag was supplied by the last successful parse.
- `values` returns raw, environment-expanded strings.
- `get<T>` converts the first stored value; `getAll<T>` converts all stored values.
- `reset` clears values and supplied-state markers.
- `removeArg` / `deleteArg` remove the complete definition and every alias.
- `arguments` provides a read-only list of the registrations and parsed values.

The parser is not internally synchronized; use separate instances or external
synchronization for concurrent access.

## Architecture and performance

The public API is declared across `Argument.hpp`, `Conversion.hpp`,
`Exceptions.hpp`, `Formatter.hpp`, and `Serialization.hpp`. Non-template
implementations are in `src/`, while templates remain in headers so custom
types can be instantiated by applications. `ArgParser.hpp` provides the
combined public entry point and coordinates registration, parsing, formatting,
and persistence. Registration uses a vector for ordered help and a hash map for
average constant-time flag lookup. Parsing stages results before committing
them, and keeps raw value strings to avoid repeated conversions.

Build the standalone microbenchmark with
`cmake -S . -B build -DARGPARSER_BUILD_BENCHMARKS=ON`, then run
`./build/argparser_benchmark`. It reports parse throughput for 4, 16, 64, and
256 options, mixing text and numeric values. Results depend on compiler,
hardware, and build configuration; use a release build for comparisons.

## API reference

The complete public declarations are in `include/parser/`. Main entry points:

- `parser::ArgParser::addArg(ArgumentConfig)`
- `parser::ArgParser::arg(shortFlag, longFlag, description, callable)`
- `parser::ArgParser::parse(argc, argv)` and `parse(span<string_view>)`
- `parser::ArgParser::removeArg`, `hasArg`, `wasProvided`, `values`, `get`,
  `getAll`, and `reset`
- `parser::ArgParser::usage`, `printHelp`, `save`, and `read`
- `parser::Converter<T>` for custom conversion
