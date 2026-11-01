#include "parser/ArgParser.hpp"

#include "parser/Formatter.hpp"
#include "parser/Serialization.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <functional>
#include <limits>
#include <utility>

namespace parser
{

struct ArgParser::Occurrence
{
	std::vector<std::string> values;
};

ArgParser::ArgParser() = default;

void ArgParser::addArg(ArgumentConfig config)
{
	validate_definition(config);

	Argument argument;
	argument.shortFlag = std::move(config.shortFlag);
	argument.longFlag = std::move(config.longFlag);
	argument.description = std::move(config.description);
	argument.required = config.required;
	argument.allowRepeated = config.allowRepeated;
	argument.defaultValue = std::move(config.defaultValue);
	argument.target = std::move(config.target);
	argument.typeName = argument.target.configured() ? std::string(argument.target.type_name()) : "STRING";
	argument.minValues = config.minValues == Target::unspecified
							 ? argument.target.default_min_values()
							 : config.minValues;
	argument.maxValues = config.maxValues == Target::unspecified
							 ? argument.target.default_max_values()
							 : config.maxValues;
	if (argument.maxValues != Target::unspecified && argument.minValues > argument.maxValues)
		throw invalid_argument_definition("minimum value count exceeds maximum value count for " + canonical_flag(argument));
	if (argument.target.configured() && argument.target.type_name() == "FLAG" && (argument.minValues > 0 || argument.maxValues > 0))
		throw invalid_argument_definition("no-value callbacks cannot configure value counts");

	const auto index = arguments_.size();
	if (!argument.shortFlag.empty())
		lookup_.emplace(argument.shortFlag, index);
	if (!argument.longFlag.empty())
		lookup_.emplace(argument.longFlag, index);
	arguments_.push_back(std::move(argument));
}

bool ArgParser::removeArg(std::string_view flag)
{
	const auto found = lookup_.find(std::string(flag));
	if (found == lookup_.end())
		return false;
	arguments_.erase(arguments_.begin() + static_cast<std::ptrdiff_t>(found->second));
	rebuild_lookup();
	return true;
}

bool ArgParser::deleteArg(std::string_view flag)
{
	return removeArg(flag);
}

bool ArgParser::hasArg(std::string_view flag) const
{
	return lookup_.find(std::string(flag)) != lookup_.end();
}

bool ArgParser::exists(std::string_view flag) const
{
	return hasArg(flag);
}

bool ArgParser::wasProvided(std::string_view flag) const
{
	return arguments_[index_for(flag)].supplied;
}

const std::vector<std::string> &ArgParser::values(std::string_view flag) const
{
	return arguments_[index_for(flag)].values;
}

const std::vector<Argument> &ArgParser::arguments() const noexcept
{
	return arguments_;
}

void ArgParser::reset() noexcept
{
	for (auto &argument : arguments_)
	{
		argument.supplied = false;
		argument.values.clear();
	}
}

bool ArgParser::parse(int argc, char **argv)
{
	try
	{
		if (argc < 0 || (argc > 0 && argv == nullptr))
			throw invalid_argument_definition("argc and argv do not describe a valid argument list");
		std::vector<std::string_view> tokens;
		tokens.reserve(argc > 0 ? static_cast<std::size_t>(argc - 1) : 0);
		for (int i = 1; i < argc; ++i)
		{
			if (argv[i] == nullptr)
				throw invalid_argument_definition("argv contains a null argument");
			tokens.emplace_back(argv[i]);
		}
		return parse_tokens(std::span<const std::string_view>(tokens));
	}
	catch (const error &)
	{
		if (automaticUsage_)
			std::cerr << usage();
		throw;
	}
}

bool ArgParser::parse(std::span<const std::string_view> tokens)
{
	try
	{
		return parse_tokens(tokens);
	}
	catch (const error &)
	{
		if (automaticUsage_)
			std::cerr << usage();
		throw;
	}
}

bool ArgParser::parse(const std::vector<std::string> &tokens)
{
	std::vector<std::string_view> views;
	views.reserve(tokens.size());
	for (const auto &token : tokens)
		views.emplace_back(token);
	return parse(std::span<const std::string_view>(views));
}

void ArgParser::setApplicationName(std::string name)
{
	applicationName_ = std::move(name);
}

void ArgParser::setApplicationDescription(std::string description)
{
	applicationDescription_ = std::move(description);
}

void ArgParser::addExample(std::string example)
{
	examples_.push_back(std::move(example));
}

void ArgParser::setColorsEnabled(bool enabled) noexcept
{
	colorsEnabled_ = enabled;
}

void ArgParser::setAutomaticUsage(bool enabled) noexcept
{
	automaticUsage_ = enabled;
}

bool ArgParser::automaticUsage() const noexcept
{
	return automaticUsage_;
}

std::string ArgParser::usage() const
{
	auto visibleArguments = arguments_;
	const bool hasShortHelp = hasArg("-h");
	const bool hasLongHelp = hasArg("--help");
	if (!hasShortHelp || !hasLongHelp)
	{
		Argument help;
		if (!hasShortHelp)
			help.shortFlag = "-h";
		if (!hasLongHelp)
			help.longFlag = "--help";
		help.description = "Display this help message";
		help.typeName = "FLAG";
		visibleArguments.push_back(std::move(help));
	}
	return Formatter::format(visibleArguments, applicationName_, applicationDescription_, examples_, colorsEnabled_);
}

void ArgParser::printHelp(std::ostream &output) const
{
	output << usage();
}

void ArgParser::save(const std::string &path) const
{
	detail::write_binary_file(path, detail::serialize_state(arguments_));
}

void ArgParser::read(const std::string &path)
{
	const auto states = detail::deserialize_state(detail::read_binary_file(path));
	if (states.size() != arguments_.size())
		throw invalid_binary_format("binary state does not match the registered arguments");

	std::vector<detail::PersistedState> staged(arguments_.size());
	std::vector<bool> matched(arguments_.size(), false);
	for (const auto &[key, state] : states)
	{
		const auto found = lookup_.find(key);
		if (found == lookup_.end())
			throw invalid_binary_format("binary state references an unregistered argument: " + key);
		const auto index = found->second;
		const auto &argument = arguments_[index];
		if (key != canonical_flag(argument) || matched[index])
			throw invalid_binary_format("binary state contains a conflicting argument identity: " + key);
		if (state.shortFlag != argument.shortFlag || state.longFlag != argument.longFlag)
			throw invalid_binary_format("binary state aliases do not match the registration: " + key);
		if (!state.supplied && !state.values.empty())
			throw invalid_binary_format("unsupplied argument has serialized values: " + key);
		if (argument.required && !state.supplied)
			throw invalid_binary_format("binary state omits required argument: " + key);
		if (state.supplied)
		{
			if ((!argument.allowRepeated && (state.values.size() < argument.minValues || (argument.maxValues != Target::unspecified && state.values.size() > argument.maxValues))) || (argument.allowRepeated && argument.minValues > 0 && state.values.empty()))
				throw invalid_binary_format("serialized value count is invalid for " + key);
			try
			{
				if (argument.allowRepeated)
				{
					if (state.values.empty())
						argument.target.validate(state.values);
					for (const auto &value : state.values)
						argument.target.validate(std::vector<std::string>{value});
				}
				else
				{
					argument.target.validate(state.values);
				}
			}
			catch (const error &exception)
			{
				throw invalid_binary_format("serialized value is invalid for " + key + ": " + exception.what());
			}
		}
		staged[index] = state;
		matched[index] = true;
	}

	if (std::find(matched.begin(), matched.end(), false) != matched.end())
		throw invalid_binary_format("binary state is missing registered arguments");
	for (std::size_t i = 0; i < arguments_.size(); ++i)
	{
		arguments_[i].supplied = staged[i].supplied;
		arguments_[i].values = std::move(staged[i].values);
	}
}

std::string ArgParser::canonical_flag(const Argument &argument)
{
	return argument.longFlag.empty() ? argument.shortFlag : argument.longFlag;
}

void ArgParser::validate_flag(const std::string &flag, bool shortFlag)
{
	if (flag.empty())
		return;
	if (std::any_of(flag.begin(), flag.end(), [](unsigned char character)
					{ return std::isspace(character) || character == '='; }))
		throw invalid_argument_definition("flags cannot contain whitespace or '=': " + flag);
	if (shortFlag)
	{
		if (flag.size() != 2 || flag[0] != '-' || flag[1] == '-' || flag[1] == '=')
			throw invalid_argument_definition("short flags must have the form -x: " + flag);
	}
	else
	{
		if (flag.size() < 3 || flag[0] != '-' || flag[1] != '-' || flag[2] == '-' || flag.find('=') != std::string::npos)
			throw invalid_argument_definition("long flags must have the form --name: " + flag);
	}
}

void ArgParser::validate_definition(const ArgumentConfig &config) const
{
	if (config.shortFlag.empty() && config.longFlag.empty())
		throw invalid_argument_definition("an argument must define a short flag, a long flag, or both");
	validate_flag(config.shortFlag, true);
	validate_flag(config.longFlag, false);
	if (!config.shortFlag.empty() && config.shortFlag == config.longFlag)
		throw invalid_argument_definition("short and long flags must be distinct");
	for (const auto &flag : {config.shortFlag, config.longFlag})
	{
		if (!flag.empty() && lookup_.find(flag) != lookup_.end())
			throw duplicate_flag("flag is already registered: " + flag);
	}
	if (config.minValues != Target::unspecified && config.maxValues != Target::unspecified && config.minValues > config.maxValues)
		throw invalid_argument_definition("minimum value count exceeds maximum value count");
}

std::size_t ArgParser::index_for(std::string_view flag) const
{
	const auto found = lookup_.find(std::string(flag));
	if (found == lookup_.end())
		throw unknown_argument("argument is not registered: " + std::string(flag));
	return found->second;
}

void ArgParser::rebuild_lookup()
{
	lookup_.clear();
	for (std::size_t i = 0; i < arguments_.size(); ++i)
	{
		if (!arguments_[i].shortFlag.empty())
			lookup_.emplace(arguments_[i].shortFlag, i);
		if (!arguments_[i].longFlag.empty())
			lookup_.emplace(arguments_[i].longFlag, i);
	}
}

bool ArgParser::looks_like_flag(std::string_view token)
{
	return token.size() > 1 && token.front() == '-';
}

bool ArgParser::is_negative_number(std::string_view token)
{
	if (token.size() < 2 || token.front() != '-')
		return false;
	double value{};
	const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
	return result.ec == std::errc{} && result.ptr == token.data() + token.size();
}

std::string ArgParser::expand_environment(std::string_view input)
{
	std::string output;
	output.reserve(input.size());
	for (std::size_t i = 0; i < input.size();)
	{
		if (input[i] != '$')
		{
			output.push_back(input[i++]);
			continue;
		}
		++i;
		bool braced = i < input.size() && input[i] == '{';
		if (braced)
			++i;
		const auto nameStart = i;
		if (i >= input.size() || !(std::isalpha(static_cast<unsigned char>(input[i])) || input[i] == '_'))
		{
			output.push_back('$');
			if (braced)
				output.push_back('{');
			continue;
		}
		++i;
		while (i < input.size() && (std::isalnum(static_cast<unsigned char>(input[i])) || input[i] == '_'))
			++i;
		const std::string name(input.substr(nameStart, i - nameStart));
		if (braced)
		{
			if (i >= input.size() || input[i] != '}')
				throw invalid_value("unterminated environment variable expansion in '" + std::string(input) + "'");
			++i;
		}
		const char *value = std::getenv(name.c_str());
		if (value == nullptr)
			throw invalid_value("environment variable is not set: " + name);
		output += value;
	}
	return output;
}

bool ArgParser::is_negative_value_for(const Argument &argument, std::string_view token) const
{
	return (argument.typeName == "INTEGER" || argument.typeName == "FLOAT") && is_negative_number(token);
}

bool ArgParser::parse_tokens(std::span<const std::string_view> tokens)
{
	std::vector<std::vector<Occurrence>> staged(arguments_.size());
	for (std::size_t i = 0; i < tokens.size(); ++i)
	{
		const auto token = tokens[i];
		if ((token == "-h" || token == "--help") && lookup_.find(std::string(token)) == lookup_.end())
		{
			printHelp();
			return false;
		}
		if (token.empty() || token.front() != '-')
			throw unknown_argument("unexpected positional argument: " + std::string(token));

		const auto equals = token.find('=');
		const auto flag = token.substr(0, equals);
		const auto found = lookup_.find(std::string(flag));
		if (found == lookup_.end())
			throw unknown_argument("unknown argument: " + std::string(flag));
		const auto index = found->second;
		const auto &argument = arguments_[index];
		auto &occurrences = staged[index];
		if (!occurrences.empty() && !argument.allowRepeated)
			throw invalid_value("argument was supplied more than once: " + canonical_flag(argument));

		Occurrence occurrence;
		if (equals != std::string_view::npos)
		{
			if (argument.maxValues == 0)
				throw invalid_value("argument does not accept a value: " + canonical_flag(argument));
			occurrence.values.push_back(expand_environment(token.substr(equals + 1)));
		}
		else if (argument.maxValues > 0)
		{
			if (i + 1 < tokens.size() && !looks_like_flag(tokens[i + 1]))
			{
				occurrence.values.push_back(expand_environment(tokens[++i]));
			}
			else if (i + 1 < tokens.size() && is_negative_value_for(argument, tokens[i + 1]))
			{
				occurrence.values.push_back(expand_environment(tokens[++i]));
			}
			else if (argument.minValues > 0)
			{
				throw missing_argument_value("missing value for argument: " + canonical_flag(argument));
			}

			while (occurrence.values.size() < argument.maxValues && i + 1 < tokens.size())
			{
				const auto next = tokens[i + 1];
				if (looks_like_flag(next) && !is_negative_value_for(argument, next))
					break;
				occurrence.values.push_back(expand_environment(tokens[++i]));
			}
		}

		if (occurrence.values.size() < argument.minValues || (argument.maxValues != Target::unspecified && occurrence.values.size() > argument.maxValues))
			throw missing_argument_value("argument " + canonical_flag(argument) + " received an invalid number of values");
		try
		{
			argument.target.validate(occurrence.values);
		}
		catch (const invalid_value &exception)
		{
			throw invalid_value(canonical_flag(argument) + ": " + exception.what());
		}
		occurrences.push_back(std::move(occurrence));
	}

	for (std::size_t index = 0; index < arguments_.size(); ++index)
	{
		const auto &argument = arguments_[index];
		if (argument.required && staged[index].empty())
			throw required_argument_missing("required argument is missing: " + canonical_flag(argument));
	}

	std::vector<std::vector<std::string>> parsedValues(arguments_.size());
	for (std::size_t index = 0; index < arguments_.size(); ++index)
	{
		auto &values = parsedValues[index];
		for (const auto &occurrence : staged[index])
			values.insert(values.end(), occurrence.values.begin(), occurrence.values.end());
	}
	for (std::size_t index = 0; index < arguments_.size(); ++index)
	{
		arguments_[index].supplied = !staged[index].empty();
		arguments_[index].values.swap(parsedValues[index]);
	}
	for (std::size_t index = 0; index < arguments_.size(); ++index)
		for (const auto &occurrence : staged[index])
			arguments_[index].target.invoke(occurrence.values);
	return true;
}

} // namespace parser
