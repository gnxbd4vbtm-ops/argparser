#include "parser/Exceptions.hpp"
#include <parser/ArgParser.hpp>

#include <iostream>
#include <string>

int main(int argc, char **argv)
{
	parser::ArgParser arguments;
	arguments.setApplicationName("greet");
	arguments.setApplicationDescription("Print a personalized greeting.");
	arguments.addExample("greet --name Mia");

	std::string name = "world";
	arguments.addArg({.shortFlag = "-n", .longFlag = "--name", .description = "Name to greet", .target = &name, .defaultValue = "world"});
	arguments.arg("-v", "--verbose", "Print the selected name", [&]
				  { std::cout << "Using name: " << name << '\n'; });

	try
	{
		if (!arguments.parse(argc, argv))
			return 0;
		std::cout << "Hello, " << name << "!\n";
	}
	catch (const parser::error &exception)
	{
		std::cerr << exception.what() << '\n';
		return 1;
	}
}
