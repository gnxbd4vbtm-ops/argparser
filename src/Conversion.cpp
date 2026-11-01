#include "parser/Conversion.hpp"

namespace parser
{

std::string Converter<std::string>::convert(std::string_view text)
{
	return std::string(text);
}

std::string_view Converter<std::string_view>::convert(std::string_view text) noexcept
{
	return text;
}

bool Converter<bool>::convert(std::string_view text)
{
	if (text == "true" || text == "1")
		return true;
	if (text == "false" || text == "0")
		return false;
	throw invalid_value("cannot convert '" + std::string(text) + "' to BOOLEAN (expected true, false, 1, or 0)");
}

std::filesystem::path Converter<std::filesystem::path>::convert(std::string_view text)
{
	return std::filesystem::path(std::string(text));
}

} // namespace parser
