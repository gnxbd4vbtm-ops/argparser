#include <parser/ArgParser.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

int main()
{
    constexpr int iterations = 100000;
    for (const int argumentCount : {4, 16, 64, 256})
    {
        parser::ArgParser parser;
        parser.setAutomaticUsage(false);
        std::vector<std::string> tokens;
        tokens.reserve(static_cast<std::size_t>(argumentCount) * 2);
        for (int index = 0; index < argumentCount; ++index)
        {
            const auto flag = "--value" + std::to_string(index);
            if (index % 2 == 0)
                parser.addArg({.longFlag = flag, .target = [](int) {}});
            else
                parser.addArg({.longFlag = flag, .target = [](const std::string&) {}});
            tokens.push_back(flag);
            tokens.push_back(index % 2 == 0 ? std::to_string(index) : "text");
        }
        const auto start = std::chrono::steady_clock::now();
        for (int iteration = 0; iteration < iterations; ++iteration)
            parser.parse(tokens);
        const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << argumentCount << " arguments: " << (iterations / elapsed)
                  << " parses/second (" << iterations << " iterations)\n";
    }
}
