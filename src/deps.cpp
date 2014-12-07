#include "deps.hpp"

#include <ctime>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "Poco/Process.h"
#include "Poco/Pipe.h"
#include "Poco/PipeStream.h"
#include <boost/filesystem.hpp>

#include "filesystem_utils.hpp"

using namespace std;
using namespace boost::filesystem;

vector<path> get_src_deps(Environment const& env, path srcfile)
{
    vector<path> deps;
    string word;

    stringstream args;

    args << "-MM -MT \"\" ";
    args << env.cxxflags << " ";
    args << srcfile;

    Poco::Pipe outPipe;
    Poco::PipeInputStream pin (outPipe);
    auto handle = Poco::Process::launch(env.cxx, {args.str()}, nullptr, &outPipe, nullptr);

    while (pin >> word)
    {
        if (word != ":" && word != "\\")
            deps.push_back(word);
    }

    transform(begin(deps), end(deps), begin(deps), normalize);

    return deps;
}

vector<path> read_dep_file(path depfile)
{
    vector<path> rv;
    ifstream in (depfile.string());
    string line;

    while (getline(in, line))
    {
        rv.push_back(line);
    }

    return rv;
}

void write_dep_file(path f, vector<path> const& deps)
{
    ofstream depfile (f.string());

    for (auto&& p : deps)
    {
        depfile << p.string() << endl;
    }
}

DepData process_dep_file(Environment const& env, path ent)
{
    DepData rv;

    path deppath = env.dep/ent;
    deppath.replace_extension(".dep");
    deppath = normalize(deppath);

    bool deppath_exists = exists(deppath);

    auto make_it = [&]
    {
        auto newdeps = get_src_deps(env, env.src/ent);
        write_dep_file(deppath, newdeps);
        return newdeps;
    };

    auto deps = [&]
    {
        if (!deppath_exists)
        {
            return make_it();
        }
        else
        {
            return read_dep_file(deppath);
        }
    }();

    auto target_time = last_write_time(deppath);

    bool reparse = false;

    for (auto&& f : deps)
    {
        bool f_exists = exists(f);
        time_t f_time = (f_exists?last_write_time(f):time_t{});

        bool f_newer = f_time>target_time;

        if (!f_exists || f_newer)
        {
            deps = make_it();
            reparse = true;
            rv.newest = time_t{};
            break;
        }
        else
        {
            if (f_time > rv.newest)
                rv.newest = f_time;
        }
    }

    if (reparse)
    {
        for (auto&& f : deps)
        {
            bool f_exists = exists(f);
            time_t f_time = (f_exists?last_write_time(f):time_t{});

            if (!f_exists)
            {
                rv.missing_dep = true;
            }
            else
            {
                if (f_time > rv.newest)
                    rv.newest = f_time;
            }
        }
    }

    return rv;
}
