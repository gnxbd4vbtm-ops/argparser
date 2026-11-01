#pragma once

#include "Conversion.hpp"

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace parser
{

namespace detail
{

template <typename T>
struct is_vector : std::false_type
{
};

template <typename T, typename Allocator>
struct is_vector<std::vector<T, Allocator>> : std::true_type
{
	using value_type = T;
};

template <typename T>
struct callable_traits : callable_traits<decltype(&T::operator())>
{
};

template <typename R, typename... Args>
struct callable_traits<R (*)(Args...)>
{
	static constexpr std::size_t arity = sizeof...(Args);
	template <std::size_t Index>
	using argument = std::tuple_element_t<Index, std::tuple<Args...>>;
};

template <typename R, typename... Args>
struct callable_traits<R (*)(Args...) noexcept> : callable_traits<R (*)(Args...)>
{
};

template <typename R, typename... Args>
struct callable_traits<R(Args...)> : callable_traits<R (*)(Args...)>
{
};

template <typename R, typename... Args>
struct callable_traits<R(Args...) noexcept> : callable_traits<R (*)(Args...)>
{
};

template <typename C, typename R, typename... Args>
struct callable_traits<R (C::*)(Args...) const> : callable_traits<R (*)(Args...)>
{
};

template <typename C, typename R, typename... Args>
struct callable_traits<R (C::*)(Args...)> : callable_traits<R (*)(Args...)>
{
};

template <typename C, typename R, typename... Args>
struct callable_traits<R (C::*)(Args...) const noexcept> : callable_traits<R (*)(Args...)>
{
};

template <typename C, typename R, typename... Args>
struct callable_traits<R (C::*)(Args...) noexcept> : callable_traits<R (*)(Args...)>
{
};

template <typename T>
using clean_t = std::remove_cv_t<std::remove_reference_t<T>>;

} // namespace detail

class Target
{
  public:
	static constexpr std::size_t unspecified = std::numeric_limits<std::size_t>::max();

	Target() = default;
	Target(const Target &) = default;
	Target(Target &&) noexcept = default;

	template <typename T>
		requires(!std::is_same_v<detail::clean_t<T>, Target>)
	Target(T &&value)
	{
		*this = std::forward<T>(value);
	}

	Target &operator=(const Target &) = default;
	Target &operator=(Target &&) noexcept = default;

	template <typename T>
		requires(!std::is_same_v<detail::clean_t<T>, Target>)
	Target &operator=(T &&value)
	{
		using U = detail::clean_t<T>;
		if constexpr (std::is_pointer_v<U> && !std::is_function_v<std::remove_pointer_t<U>>)
		{
			using Value = std::remove_cv_t<std::remove_pointer_t<U>>;
			static_assert(!std::is_const_v<std::remove_pointer_t<U>>,
						  "parser variable targets must point to a mutable value");
			auto *pointer = value;
			configure<Value>([pointer](const std::vector<std::string> &values)
							 {
                if constexpr (detail::is_vector<Value>::value)
                {
                    using Element = typename detail::is_vector<Value>::value_type;
                    Value converted;
                    converted.reserve(values.size());
                    for (const auto& item : values) converted.push_back(parser::convert<Element>(item));
                    *pointer = std::move(converted);
                }
                else
                {
                    if constexpr (std::is_same_v<Value, bool>)
                        *pointer = values.empty() ? true : parser::convert<bool>(values.front());
                    else if (!values.empty())
                        *pointer = parser::convert<Value>(values.front());
                } });
		}
		else if constexpr (std::is_invocable_v<U &>)
		{
			auto callable = std::make_shared<U>(std::forward<T>(value));
			typeName_ = "FLAG";
			defaultMin_ = defaultMax_ = 0;
			validate_ = [](const std::vector<std::string> &) {};
			invoke_ = [callable](const std::vector<std::string> &)
			{ std::invoke(*callable); };
		}
		else
		{
			using Traits = detail::callable_traits<U>;
			static_assert(Traits::arity == 1, "targets must accept no arguments or exactly one typed value");
			using Parameter = detail::clean_t<typename Traits::template argument<0>>;
			auto callable = std::make_shared<U>(std::forward<T>(value));
			if constexpr (detail::is_vector<Parameter>::value)
			{
				using Element = typename detail::is_vector<Parameter>::value_type;
				typeName_ = std::string(Converter<Element>::type_name());
				defaultMin_ = 1;
				defaultMax_ = unspecified;
				validate_ = [](const std::vector<std::string> &values)
				{
					for (const auto &item : values)
						(void)parser::convert<Element>(item);
				};
				invoke_ = [callable](const std::vector<std::string> &values)
				{
					Parameter converted;
					converted.reserve(values.size());
					for (const auto &item : values)
						converted.push_back(parser::convert<Element>(item));
					std::invoke(*callable, std::move(converted));
				};
			}
			else
			{
				typeName_ = std::string(Converter<Parameter>::type_name());
				if constexpr (std::is_same_v<Parameter, bool>)
				{
					defaultMin_ = 0;
					defaultMax_ = 1;
					validate_ = [](const std::vector<std::string> &values)
					{
						if (!values.empty())
							(void)parser::convert<bool>(values.front());
					};
				}
				else
				{
					defaultMin_ = defaultMax_ = 1;
					validate_ = [](const std::vector<std::string> &values)
					{
						if (!values.empty())
							(void)parser::convert<Parameter>(values.front());
					};
				}
				invoke_ = [callable](const std::vector<std::string> &values)
				{
					if constexpr (std::is_same_v<Parameter, bool>)
						std::invoke(*callable, values.empty() ? true : parser::convert<bool>(values.front()));
					else
					{
						if (!values.empty())
							std::invoke(*callable, parser::convert<Parameter>(values.front()));
					}
				};
			}
		}
		return *this;
	}

	bool configured() const noexcept;
	std::string_view type_name() const noexcept;
	std::size_t default_min_values() const noexcept;
	std::size_t default_max_values() const noexcept;

	void validate(const std::vector<std::string> &values) const;
	void invoke(const std::vector<std::string> &values) const;

  private:
	template <typename Value, typename Function>
	void configure(Function &&function)
	{
		if constexpr (detail::is_vector<Value>::value)
		{
			using Element = typename detail::is_vector<Value>::value_type;
			typeName_ = std::string(Converter<Element>::type_name());
			defaultMin_ = 1;
			defaultMax_ = unspecified;
			validate_ = [](const std::vector<std::string> &values)
			{
				for (const auto &item : values)
					(void)parser::convert<Element>(item);
			};
		}
		else
		{
			typeName_ = std::string(Converter<Value>::type_name());
			if constexpr (std::is_same_v<Value, bool>)
			{
				defaultMin_ = 0;
				defaultMax_ = 1;
				validate_ = [](const std::vector<std::string> &values)
				{
					if (!values.empty())
						(void)parser::convert<bool>(values.front());
				};
			}
			else
			{
				defaultMin_ = defaultMax_ = 1;
				validate_ = [](const std::vector<std::string> &values)
				{
					if (!values.empty())
						(void)parser::convert<Value>(values.front());
				};
			}
		}
		invoke_ = std::forward<Function>(function);
	}

	std::string typeName_;
	std::size_t defaultMin_ = 0;
	std::size_t defaultMax_ = 0;
	std::function<void(const std::vector<std::string> &)> validate_;
	std::function<void(const std::vector<std::string> &)> invoke_;
};

struct ArgumentConfig
{
	std::string shortFlag;
	std::string longFlag;
	std::string description;
	bool required = false;
	Target target;
	std::size_t minValues = Target::unspecified;
	std::size_t maxValues = Target::unspecified;
	bool allowRepeated = false;
	std::string defaultValue;
};

struct Argument
{
	std::string shortFlag;
	std::string longFlag;
	std::string description;
	std::string typeName;
	std::string defaultValue;
	bool required = false;
	bool allowRepeated = false;
	std::size_t minValues = 0;
	std::size_t maxValues = 0;
	bool supplied = false;
	std::vector<std::string> values;

  private:
	friend class ArgParser;
	Target target;
};

} // namespace parser
