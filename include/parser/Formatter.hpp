#pragma once

#include "Argument.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace parser
{

class Formatter
{
  public:
	static std::string format(const std::vector<Argument> &arguments,
							  const std::string &applicationName,
							  const std::string &applicationDescription,
							  const std::vector<std::string> &examples,
							  bool useColors);

  private:
	static void write_wrapped(std::ostringstream &output, const std::string &prefix,
							  const std::string &text, std::size_t column, std::size_t width);

	static std::string flags(const Argument &argument);
};

} // namespace parser
