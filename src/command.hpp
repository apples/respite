#ifndef COMMAND_HPP
#define COMMAND_HPP

#include <string>

struct CommandResult
{
    std::string err;
    bool success = true;
};

CommandResult run_command(std::string const& exe, std::string const& args);

#endif // COMMAND_HPP
