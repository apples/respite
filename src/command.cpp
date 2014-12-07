#include "command.hpp"

#include "Poco/Pipe.h"
#include "Poco/PipeStream.h"
#include "Poco/Process.h"

#include <string>
#include <sstream>

CommandResult run_command(std::string const& exe, std::string const& args)
{
    CommandResult rv;

    Poco::Pipe errPipe;
    Poco::PipeInputStream errStream (errPipe);
    auto handle = Poco::Process::launch(exe, {args}, nullptr, nullptr, &errPipe);

    std::string line;
    std::ostringstream ss;

    while (std::getline(errStream,line))
    {
        ss << line << "\n";
    }

    rv.err = ss.str();

    auto exit_code = handle.wait();

    rv.success = (exit_code == 0);

    return rv;
}
