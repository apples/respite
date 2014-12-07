#include "build.hpp"

#include <sstream>
#include <string>
#include <vector>
#include <boost/filesystem.hpp>

#include "command.hpp"
#include "filesystem_utils.hpp"

using namespace std;
using namespace boost::filesystem;

CommandResult build_obj(Environment const& env, path f)
{
    stringstream args_ss;

    path objfile = env.obj/f;
    objfile.replace_extension(".o");
    objfile = normalize(objfile);

    path srcfile = normalize(env.src/f);

    args_ss << env.cppflags << " ";
    args_ss << env.cxxflags << " ";
    args_ss << "-c " << srcfile << " ";
    args_ss << "-o " << objfile << " ";

    string args = args_ss.str();

    return run_command(env.cxx, args);
}

CommandResult build_exe(Environment const& env, path exe, vector<path> const& objs)
{
    stringstream args_ss;

    args_ss << env.ldflags << " ";
    for (auto&& p : objs)
        args_ss << normalize(p) << " ";
    args_ss << env.ldlibs << " ";
    args_ss << "-o " << normalize(exe) << " ";

    string args = args_ss.str();

    return run_command(env.cxx, args);
}
