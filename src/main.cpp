#include <algorithm>
#include <iterator>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include <exec-stream.h>

using namespace std;
using namespace boost;
using namespace boost::filesystem;

template <typename T>
string make_string(T&& t)
{
    stringstream ss;
    ss << t;
    return ss.str();
}

string const& make_string(string const& str)
{
    return str;
}

template <typename... Ts>
void dbg(Ts&&... us)
{
    vector<string> strs = {
        make_string(us)...
    };
    for (auto&& str : strs)
        clog << str;
    clog << endl;
}

vector<directory_entry> recursive_list(path p)
{
    vector<directory_entry> rv;

    auto not_dotfile = [](directory_entry const& ent)
    {
        return (ent.path().filename().string()[0] != '.');
    };

    auto dir_begin = recursive_directory_iterator(p);
    auto dir_end   = recursive_directory_iterator();

    copy_if(dir_begin, dir_end, back_inserter(rv), not_dotfile);

    return rv;
}

class _scope_cd_type
{
    path prev;

    public:

        _scope_cd_type(path p)
            : prev(current_path())
        {
            current_path(p);
        }

        ~_scope_cd_type()
        {
            current_path(prev);
        }
};

_scope_cd_type push_dir(path p)
{
    return _scope_cd_type(p);
}

vector<path> get_src_deps(path srcfile, string const& cxxflags)
{
    vector<path> deps;
    string word;

    stringstream args;

    args << "-MM -MT \"\" ";
    args << cxxflags << " ";
    args << srcfile;
    
    exec_stream_t es;
    es.set_wait_timeout(exec_stream_t::s_all, 60000);
    es.start("g++", args.str());

    while (es.out() >> word)
    {
        if (word != ":" && word != "\\")
            deps.push_back(word);
    }

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
        depfile << p << endl;
    }
}

struct DepData
{
    bool needs_remake = true;
    time_t newest = 0;
};

DepData process_dep_file(path src, path f, path dep, string const& cxxflags)
{
    DepData rv;

    path deppath = dep/f;
    deppath.replace_extension(".dep");

    auto make_it = [&]
    {
        dbg("Creating dep file ", deppath, "...");
        auto newdeps = get_src_deps(src/f, cxxflags);
        write_dep_file(deppath, newdeps);
    };

    auto get_time = [&](path p)
    {
        auto t = last_write_time(p);
        if (t>rv.newest) rv.newest = t;
        return t;
    };
    
    if (exists(deppath))
    {
        auto target_time = last_write_time(deppath);
        auto deps = read_dep_file(deppath);

        for (auto&& f : deps)
        {
            if (!exists(f) || get_time(f)>target_time)
            {
                make_it();
                return rv;
            }
        }
    }
    else
    {
        make_it();
        return rv;
    }

    rv.needs_remake = false;
    return rv;
}

bool build_obj(path src, path f, path obj, string const& cxxflags)
{
    stringstream args;

    path objfile = obj/f;
    objfile.replace_extension(".o");

    args << cxxflags << " ";
    args << "-c " << src/f << " ";
    args << "-o " << objfile << " ";
    
    exec_stream_t es;
    es.set_wait_timeout(exec_stream_t::s_all, 60000);
    es.set_buffer_limit(exec_stream_t::s_all, 0);
    es.start("g++", args.str());

    cerr << es.out();

    while (!es.close());

    return (es.exit_code() == 0);
}

bool build_exe(path exe, vector<path> const& objs, string const& ldflags)
{
    stringstream args;

    args << ldflags << " ";
    for (auto&& p : objs)
        args << p << " ";
    args << "-o " << exe << " ";
    
    exec_stream_t es;
    es.set_wait_timeout(exec_stream_t::s_all, 60000);
    es.set_buffer_limit(exec_stream_t::s_all, 0);
    es.start("g++", args.str());

    cerr << es.out();

    while (!es.close());

    return (es.exit_code() == 0);
}

int main(int argc, char* argv[])
{
    path base = current_path();

    path src = "./src";
    path dep = "./dep";
    path obj = "./obj";
    path bin = "./bin";

    // READ COMMAND LINE

    stringstream ss_cxx;
    stringstream ss_ld;
    stringstream* ssp = &ss_cxx;

    ++argv;

    while (*argv)
    {
        if (*argv == "--")
        {
            ssp = &ss_ld;
        }
        else
        {
            *ssp << *argv << " ";
        }
    }

    string cxxflags = ss_cxx.str();
    string ldflags = ss_ld.str();

    // CREATE DIRECTORIES
    
    auto src_files = [&]
    {
        auto _ = push_dir(src);
        return recursive_list(".");
    }();

    for (auto&& ent : src_files)
    {
        if (is_directory(ent.status()))
        {
            auto make_it = [&](path tget)
            {
                tget /= ent.path();
                create_directories(tget);
            };

            make_it(dep);
            make_it(obj);
        }
    }

    create_directories(bin);

    // GENERATE MISSING/OUTDATED DEP FILES

    vector<path> objs;

    for (auto&& ent : src_files)
    {
        if (is_regular_file(ent.status()))
        {
            auto ext = ent.path().extension().string();

            path deppath = dep/ent.path();

            if (ext == ".cpp")
            {
                auto dep_data = process_dep_file(src, ent.path(), dep, cxxflags);
                path objfile = obj/ent.path();
                objfile.replace_extension(".o");

                objs.push_back(objfile);

                if (dep_data.needs_remake
                    || !exists(objfile)
                    || dep_data.newest>last_write_time(objfile))
                {
                    auto success = build_obj(src, ent.path(), obj, cxxflags);
                    if (!success)
                    {
                        cerr << endl << "BUILD FAILED" << endl;
                        return 1;
                    }
                }
            }
        }
    }

    // BUILD EXECUTABLE

    path exe = bin/"a.jout";
    bool needs_build = false;

    if (!exists(exe))
    {
        needs_build = true;
    }
    else
    {
        auto exe_time = last_write_time(exe);

        for (auto&& p : objs)
        {
            auto p_time = last_write_time(p);

            if (p_time > exe_time)
            {
                needs_build = true;
                break;
            }
        }
    }

    if (needs_build)
    {
        auto success = build_exe(exe, objs, ldflags);
        if (!success)
        {
            cerr << endl << "BUILD FAILED" << endl;
            return 1;
        }
    }

    cerr << endl << "BUILD SUCCESS" << endl;
}

