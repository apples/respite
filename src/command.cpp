#include "command.hpp"

#include <string>
#include <sstream>
#include <exec-stream.h>

using namespace std;

CommandResult run_command(string const& exe, string const& args)
{
    CommandResult rv;

    exec_stream_t es;
    es.set_wait_timeout(exec_stream_t::s_all, 60000);
    es.set_buffer_limit(exec_stream_t::s_all, 0);
    es.start(exe, args);

    string line;
    stringstream ss;

    while (getline(es.err(),line))
    {
        ss << line << endl;
    }

    rv.err = ss.str();

    while (!es.close());

    rv.success = (es.exit_code() == 0);

    return rv;
}
