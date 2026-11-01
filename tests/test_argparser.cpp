#include <parser/ArgParser.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

struct WrappedInteger
{
    int value;
};

}

template <>
struct parser::Converter<WrappedInteger>
{
    static WrappedInteger convert(std::string_view text)
    {
        return {parser::convert<int>(text)};
    }

    static constexpr std::string_view type_name() noexcept { return "WRAPPED_INTEGER"; }
};

namespace
{

template <typename Exception, typename Function>
void expect_throw(Function&& function)
{
    bool caught = false;
    try
    {
        function();
    }
    catch (const Exception&)
    {
        caught = true;
    }
    assert(caught);
}

void test_registration_and_callbacks()
{
    parser::ArgParser parser;
    parser.setAutomaticUsage(false);
    int timeout = 0;
    bool buildCalled = false;
    parser.addArg({.shortFlag = "-t", .longFlag = "--timeout", .description = "Timeout",
                   .required = true, .target = &timeout});
    parser.arg("-b", "--build", "Build", [&] { buildCalled = true; });
    parser.parse(std::vector<std::string>{"--timeout=15", "-b"});
    assert(timeout == 15);
    assert(buildCalled);
    assert(parser.wasProvided("-t"));
    assert(parser.get<int>("--timeout") == 15);
}

void test_conversion_and_boolean()
{
    parser::ArgParser parser;
    parser.setAutomaticUsage(false);
    bool verbose = false;
    double ratio = 0;
    parser.addArg({.longFlag = "--verbose", .description = "Verbose", .target = &verbose});
    parser.addArg({.longFlag = "--ratio", .description = "Ratio", .target = [&ratio](double value) { ratio = value; }});
    parser.parse(std::vector<std::string>{"--verbose", "--ratio", "-0.25"});
    assert(verbose);
    assert(ratio == -0.25);
    assert(parser.get<bool>("--verbose"));
    expect_throw<parser::invalid_value>([&] { parser.parse(std::vector<std::string>{"--ratio=no"}); });
    assert(parser.wasProvided("--verbose"));
}

void test_paths_and_custom_conversion()
{
    parser::ArgParser parser;
    parser.setAutomaticUsage(false);
    std::filesystem::path output;
    WrappedInteger custom{};
    parser.addArg({.longFlag = "--output", .target = &output});
    parser.addArg({.longFlag = "--custom", .target = [&custom](WrappedInteger value) { custom = value; }});
    parser.parse(std::vector<std::string>{"--output=build/out", "--custom=17"});
    assert(output == std::filesystem::path("build/out"));
    assert(custom.value == 17);
}

void test_optional_value_count()
{
    parser::ArgParser parser;
    parser.setAutomaticUsage(false);
    int number = 23;
    int callbackCount = 0;
    parser.addArg({.longFlag = "--number", .target = &number, .minValues = 0, .maxValues = 1});
    parser.addArg({.longFlag = "--callback", .target = [&callbackCount](int) { ++callbackCount; },
                   .minValues = 0, .maxValues = 1});
    parser.parse(std::vector<std::string>{"--number", "--callback"});
    assert(number == 23);
    assert(callbackCount == 0);
    assert(parser.wasProvided("--number"));
}

void test_multi_value_and_repetition()
{
    parser::ArgParser parser;
    parser.setAutomaticUsage(false);
    std::vector<std::string> paths;
    parser.addArg({.longFlag = "--include", .description = "Include paths",
                   .target = &paths, .minValues = 2, .maxValues = 3});
    parser.addArg({.longFlag = "--tag", .description = "Tag", .minValues = 1, .maxValues = 1,
                   .allowRepeated = true});
    parser.parse(std::vector<std::string>{"--include", "one", "two", "--tag=a", "--tag", "b"});
    assert((paths == std::vector<std::string>{"one", "two"}));
    assert((parser.values("--tag") == std::vector<std::string>{"a", "b"}));
    expect_throw<parser::missing_argument_value>([&] {
        parser.parse(std::vector<std::string>{"--include", "only-one"});
    });
}

void test_required_unknown_and_duplicates()
{
    parser::ArgParser parser;
    parser.setAutomaticUsage(false);
    parser.addArg({.shortFlag = "-r", .longFlag = "--required", .required = true});
    const auto help = parser.usage();
    assert(help.find("--required*") != std::string::npos);
    assert(help.find("[optional]") == std::string::npos);
    expect_throw<parser::required_argument_missing>([&] { parser.parse(std::vector<std::string>{}); });
    expect_throw<parser::unknown_argument>([&] { parser.parse(std::vector<std::string>{"--missing"}); });
    expect_throw<parser::duplicate_flag>([&] {
        parser.addArg({.longFlag = "--required"});
    });
    expect_throw<parser::invalid_value>([&] {
        parser.parse(std::vector<std::string>{"--required", "--required"});
    });
}

void test_removal_and_state()
{
    parser::ArgParser parser;
    parser.setAutomaticUsage(false);
    parser.addArg({.shortFlag = "-x", .longFlag = "--example"});
    parser.addArg({.longFlag = "--other"});
    assert(parser.removeArg("-x"));
    assert(!parser.hasArg("--example"));
    assert(parser.hasArg("--other"));
    assert(!parser.deleteArg("--example"));
    parser.parse(std::vector<std::string>{"--other"});
    assert(parser.wasProvided("--other"));
    parser.reset();
    assert(!parser.wasProvided("--other"));
}

void test_environment_expansion()
{
    parser::ArgParser parser;
    parser.setAutomaticUsage(false);
    parser.addArg({.longFlag = "--out", .minValues = 1, .maxValues = 1});
#ifdef _WIN32
    _putenv_s("ARGPARSER_TEST_ROOT", "C:/work");
#else
    setenv("ARGPARSER_TEST_ROOT", "/tmp/work", 1);
#endif
    parser.parse(std::vector<std::string>{"--out=${ARGPARSER_TEST_ROOT}/build"});
#ifdef _WIN32
    assert(parser.get<std::string>("--out") == "C:/work/build");
#else
    assert(parser.get<std::string>("--out") == "/tmp/work/build");
#endif
    expect_throw<parser::invalid_value>([&] {
        parser.parse(std::vector<std::string>{"--out=$ARGPARSER_NOT_DEFINED"});
    });
}

void test_help_and_empty_input()
{
    parser::ArgParser parser;
    parser.setAutomaticUsage(false);
    parser.setApplicationName("sample");
    parser.setColorsEnabled(false);
    parser.addArg({.longFlag = "--name", .description = "User name", .minValues = 1, .maxValues = 1});
    parser.addExample("sample --name Mia");
    const auto help = parser.usage();
    assert(help.find("Usage: sample") != std::string::npos);
    assert(help.find("--help") != std::string::npos);
    assert(help.find("User name") != std::string::npos);
    parser.parse(std::vector<std::string>{});
    std::ostringstream displayedHelp;
    auto* previous = std::cout.rdbuf(displayedHelp.rdbuf());
    const bool parseContinues = parser.parse(std::vector<std::string>{"--help"});
    std::cout.rdbuf(previous);
    assert(!parseContinues);
    assert(displayedHelp.str().find("Usage: sample") != std::string::npos);
}

void test_automatic_usage_on_errors()
{
    parser::ArgParser parser;
    parser.addArg({.longFlag = "--number", .target = [](int) {}});
    std::ostringstream captured;
    auto* previous = std::cerr.rdbuf(captured.rdbuf());
    try
    {
        parser.parse(std::vector<std::string>{"--number=not-a-number"});
    }
    catch (const parser::invalid_value&)
    {
    }
    std::cerr.rdbuf(previous);
    assert(captured.str().find("Usage:") != std::string::npos);
}

void test_serialization()
{
    const auto path = (std::filesystem::temp_directory_path() / "argparser-state-test.bin").string();
    parser::ArgParser parser;
    parser.setAutomaticUsage(false);
    int count = 0;
    parser.addArg({.longFlag = "--count", .target = &count});
    parser.addArg({.longFlag = "--switch", .target = [] {}});
    parser.parse(std::vector<std::string>{"--count=42", "--switch"});
    parser.save(path);

    parser::ArgParser restored;
    restored.setAutomaticUsage(false);
    int restoredCount = 0;
    restored.addArg({.longFlag = "--count", .target = &restoredCount});
    restored.addArg({.longFlag = "--switch", .target = [] {}});
    restored.read(path);
    assert(restored.get<int>("--count") == 42);
    assert(restored.wasProvided("--switch"));
    assert(restoredCount == 0);

    {
        std::ofstream corrupt(path, std::ios::binary | std::ios::trunc);
        corrupt << "broken";
    }
    expect_throw<parser::invalid_binary_format>([&] { restored.read(path); });
    std::remove(path.c_str());
}

void test_large_registration()
{
    parser::ArgParser parser;
    parser.setAutomaticUsage(false);
    for (int i = 0; i < 500; ++i)
        parser.addArg({.longFlag = "--option" + std::to_string(i)});
    parser.parse(std::vector<std::string>{"--option499"});
    assert(parser.wasProvided("--option499"));
    assert(parser.arguments().size() == 500);
}

}

int main()
{
    test_registration_and_callbacks();
    test_conversion_and_boolean();
    test_paths_and_custom_conversion();
    test_optional_value_count();
    test_multi_value_and_repetition();
    test_required_unknown_and_duplicates();
    test_removal_and_state();
    test_environment_expansion();
    test_help_and_empty_input();
    test_automatic_usage_on_errors();
    test_serialization();
    test_large_registration();
    std::cout << "All ArgParser tests passed\n";
}
