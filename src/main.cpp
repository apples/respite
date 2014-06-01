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

path normalize(path source)
{
    path rv;

    for (path p : source)
    {
        if (p == ".")
        {
            // nope
        }
        else if (p == "..")
        {
            if (rv.empty() || is_symlink(rv))
            {
                rv /= p;
            }
            else
            {
                rv.remove_filename();
            }
        }
        else
        {
            rv /= p;
        }
    }

    return rv;
}

template <typename T>
string make_string(T&& t)
{
    stringstream ss;
    ss << t;
    return ss.str();
}

string make_string(string str)
{
    return str;
}

template <typename... Ts>
void jecho(Ts&&... us)
{
    vector<string> strs = {
        make_string(us)...
    };
    cout << "[J] ";
    for (auto&& str : strs)
        cout << str;
    cout << endl;
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

bool run_command(string const& exe, string const& args)
{
    exec_stream_t es;
    es.set_wait_timeout(exec_stream_t::s_all, 60000);
    es.set_buffer_limit(exec_stream_t::s_all, 0);
    es.start(exe, args);

    string line;

    while (getline(es.err(),line))
    {
        cerr << line << endl;
    }

    while (!es.close());

    return (es.exit_code() == 0);
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

struct DepData
{
    bool missing_dep = false;
    time_t newest = time_t{};
};

DepData process_dep_file(path src, path f, path dep, string const& cxxflags)
{
    DepData rv;

    path deppath = dep/f;
    deppath.replace_extension(".dep");
    deppath = normalize(deppath);

    bool deppath_exists = exists(deppath);

    jecho("Parsing depfile ", deppath);
    jecho("  Depfile exists: ", (deppath_exists?"Yes":"No"));

    auto make_it = [&]
    {
        auto newdeps = get_src_deps(src/f, cxxflags);
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
            jecho("  Remaking depfile...");
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
                jecho("  Missing dep: ", f);
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

bool build_obj(path src, path f, path obj, string const& cxxflags)
{
    stringstream args_ss;

    path objfile = obj/f;
    objfile.replace_extension(".o");
    objfile = normalize(objfile);

    path srcfile = normalize(src/f);

    args_ss << cxxflags << " ";
    args_ss << "-c " << srcfile << " ";
    args_ss << "-o " << objfile << " ";

    string args = args_ss.str();

    jecho("Building object ", objfile);
    jecho("  Command: g++ ", args);
    jecho();

    return run_command("g++", args);
}

bool build_exe(path exe, vector<path> const& objs, string const& ldflags, string const& ldlibs)
{
    stringstream args_ss;

    args_ss << ldflags << " ";
    for (auto&& p : objs)
        args_ss << normalize(p) << " ";
    args_ss << ldlibs << " ";
    args_ss << "-o " << normalize(exe) << " ";

    string args = args_ss.str();

    jecho("Building executable ", normalize(exe));
    jecho("  Command: g++ ", args);
    jecho();

    return run_command("g++", args);
}

int main(int argc, char* argv[])
{
    path base = current_path();

    path src = "src";
    path dep = ".jbuild/dep";
    path obj = ".jbuild/obj";
    path bin = ".";

    // READ COMMAND LINE

    stringstream ss3[3];

    auto& ss_cxx = ss3[0];
    auto& ss_ld = ss3[1];
    auto& ss_libs = ss3[2];
    stringstream* ssp = &ss3[0];

    ++argv;

    while (*argv)
    {
        string arg = *argv++;

        if (arg == "---")
        {
            ++ssp;
        }
        else
        {
            *ssp << arg << " ";
        }
    }

    string cxxflags = ss_cxx.str();
    string ldflags = ss_ld.str();
    string ldlibs = ss_libs.str();

    // CREATE DIRECTORIES

    create_directories(dep);
    create_directories(obj);

    auto src_files = [&]
    {
        auto _ = push_dir(src);
        return recursive_list(".");
    }();

    for (auto&& ent : src_files)
    {
        normalize(ent);
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

    // GENERATE MISSING/OUTDATED DEP FILES

    vector<path> objs;
    vector<path> objs_rebuild;

    for (auto&& ent : src_files)
    {
        if (is_regular_file(ent.status()))
        {
            auto ext = ent.path().extension().string();

            path deppath = dep/ent.path();

            if (ext == ".cpp")
            {
                jecho("Found source file ", ent);

                auto dep_data = process_dep_file(src, ent.path(), dep, cxxflags);
                path objfile = obj/ent.path();
                objfile.replace_extension(".o");
                objfile = normalize(objfile);

                objs.push_back(objfile);

                bool obj_exists = exists(objfile);
                time_t obj_time = (obj_exists?last_write_time(objfile):time_t{});
                bool obj_outdated = (obj_exists?dep_data.newest>obj_time:true);

                bool obj_rebuild = false;

                if (!obj_rebuild && dep_data.missing_dep)
                {
                    jecho("Missing dependency for ", objfile);

                    jecho("BUILD FAILED");
                    return 1;
                }

                if (!obj_rebuild && !obj_exists)
                {
                    obj_rebuild = true;
                    jecho("Missing object ", objfile);
                }

                if (!obj_rebuild && obj_outdated)
                {
                    obj_rebuild = true;
                    jecho("Out-of-date object ", objfile);
                }

                if (!obj_exists || obj_outdated)
                {
                    objs_rebuild.emplace_back(ent.path());
                }

                jecho();
            }
        }
    }

    // BUILD OBJS

    if (!objs_rebuild.empty())
    {
        jecho("Rebuilding ", objs_rebuild.size(), " objects...");
        jecho();

        for (auto&& f : objs_rebuild)
        {
            auto success = build_obj(src, f, obj, cxxflags);
            if (!success)
            {
                jecho("BUILD FAILED");
                return 1;
            }
        }
    }
    else
    {
        jecho("All objects are up-to-date!");
        jecho();
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
        auto success = build_exe(exe, objs, ldflags, ldlibs);
        if (!success)
        {
            jecho("BUILD FAILED");
            return 1;
        }
    }

    jecho("BUILD SUCCESS");
}
