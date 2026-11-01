#include "parser/Formatter.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <sstream>
#include <string_view>

namespace parser
{

std::string Formatter::format(const std::vector<Argument> &arguments,
							  const std::string &applicationName,
							  const std::string &applicationDescription,
							  const std::vector<std::string> &examples,
							  bool useColors)
{
	if (std::getenv("NO_COLOR") != nullptr)
		useColors = false;
	const auto color = [useColors](const char *code, const std::string &text)
	{
		return useColors ? std::string("\033[") + code + "m" + text + "\033[0m" : text;
	};

	std::size_t flagWidth = 0;
	for (const auto &argument : arguments)
		flagWidth = std::max(flagWidth, flags(argument).size() + (argument.minValues ? argument.typeName.size() + 3 : 0));
	flagWidth = std::min<std::size_t>(flagWidth, 40);
	std::size_t terminalWidth = 80;
	if (const char *columns = std::getenv("COLUMNS"))
	{
		std::size_t configuredWidth = 0;
		const std::string_view text(columns);
		const auto result = std::from_chars(text.data(), text.data() + text.size(), configuredWidth);
		if (result.ec == std::errc{} && result.ptr == text.data() + text.size() && configuredWidth >= 40)
			terminalWidth = configuredWidth;
	}

	std::ostringstream output;
	const auto name = applicationName.empty() ? std::string("program") : applicationName;
	output << color("1;36", "Usage:") << ' ' << name << " [OPTIONS]\n";
	if (!applicationDescription.empty())
		output << applicationDescription << "\n";
	output << '\n'
		   << color("1;33", "Options:") << '\n';
	for (const auto &argument : arguments)
	{
		std::string label = flags(argument);
		if (argument.minValues > 0)
			label += " <" + (argument.typeName.empty() ? std::string("VALUE") : argument.typeName) + ">";
		std::string prefix = "  " + color("1;32", label);
		const auto descriptionColumn = std::max(flagWidth + 4, label.size() + 3);
		if (label.size() + 2 < descriptionColumn)
			prefix += std::string(descriptionColumn - label.size() - 2, ' ');
		std::string description = argument.description;
		description += argument.required ? " [required]" : " [optional]";
		if (!argument.defaultValue.empty())
			description += " [default: " + argument.defaultValue + ']';
		if (argument.minValues != argument.maxValues && argument.minValues > 0)
			description += " [values: " + std::to_string(argument.minValues) + ".." + (argument.maxValues == Target::unspecified ? "many" : std::to_string(argument.maxValues)) + ']';
		write_wrapped(output, prefix, description, descriptionColumn, terminalWidth);
	}
	if (!examples.empty())
	{
		output << '\n'
			   << color("1;33", "Examples:") << '\n';
		for (const auto &example : examples)
			output << "  " << example << '\n';
	}
	return output.str();
}

void Formatter::write_wrapped(std::ostringstream &output, const std::string &prefix,
							  const std::string &text, std::size_t column, std::size_t width)
{
	std::istringstream words(text);
	std::string word;
	std::size_t currentColumn = column;
	output << prefix;
	while (words >> word)
	{
		if (currentColumn > column && currentColumn + word.size() + 1 > width)
		{
			output << '\n'
				   << std::string(column, ' ');
			currentColumn = column;
		}
		if (currentColumn > column)
		{
			output << ' ';
			++currentColumn;
		}
		output << word;
		currentColumn += word.size();
	}
	output << '\n';
}

std::string Formatter::flags(const Argument &argument)
{
	if (argument.shortFlag.empty())
		return argument.longFlag;
	if (argument.longFlag.empty())
		return argument.shortFlag;
	return argument.shortFlag + ", " + argument.longFlag;
}

} // namespace parser
