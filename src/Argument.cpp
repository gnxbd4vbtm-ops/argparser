#include "parser/Argument.hpp"

namespace parser
{

bool Target::configured() const noexcept
{
	return static_cast<bool>(invoke_);
}

std::string_view Target::type_name() const noexcept
{
	return typeName_;
}

std::size_t Target::default_min_values() const noexcept
{
	return defaultMin_;
}

std::size_t Target::default_max_values() const noexcept
{
	return defaultMax_;
}

void Target::validate(const std::vector<std::string> &values) const
{
	if (validate_)
		validate_(values);
}

void Target::invoke(const std::vector<std::string> &values) const
{
	if (invoke_)
		invoke_(values);
}

} // namespace parser
