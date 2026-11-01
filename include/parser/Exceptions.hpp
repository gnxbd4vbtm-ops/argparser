#pragma once

#include <stdexcept>
#include <string>

namespace parser
{

class error : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class required_argument_missing : public error
{
public:
    using error::error;
};

class unknown_argument : public error
{
public:
    using error::error;
};

class missing_argument_value : public error
{
public:
    using error::error;
};

class invalid_value : public error
{
public:
    using error::error;
};

class duplicate_flag : public error
{
public:
    using error::error;
};

class invalid_argument_definition : public error
{
public:
    using error::error;
};

class file_write_error : public error
{
public:
    using error::error;
};

class file_read_error : public error
{
public:
    using error::error;
};

class invalid_binary_format : public error
{
public:
    using error::error;
};

}
