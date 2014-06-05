#include <algorithm>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <mutex>
#include <sstream>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <boost/filesystem.hpp>
#include <exec-stream.h>

using namespace std;
using namespace boost;
using namespace boost::filesystem;

char const* getenv_safe(string const& var)
{
    static map<string,char const*> defaults = {
        {"CXX","g++"},
        {"CXXFLAGS","-Wall -O2"},
        {"LDFLAGS",""},
        {"LDLIBS",""},
    };

    char const* rv = getenv(var.c_str());
    if (!rv) rv = defaults[var];
    return rv;
}

struct Environment
{
    const path base;
    const path src;
    const path dep;
    const path obj;
    const path bin;

    const string cxx;
    const string cxxflags;
    const string ldflags;
    const string ldlibs;

    Environment()
        : base(current_path())
        , src("src")
        , dep(".respite/dep")
        , obj(".respite/obj")
        , bin(".")
        , cxx(getenv_safe("CXX"))
        , cxxflags(getenv_safe("CXXFLAGS"))
        , ldflags(getenv_safe("LDFLAGS"))
        , ldlibs(getenv_safe("LDLIBS"))
    {}
};

struct CommandResult
{
    string err;
    bool success = true;
};

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
                rv /= p;
            else
                rv.remove_filename();
        }
        else
        {
            rv /= p;
        }
    }

    return rv;
}

inline void print_raw()
{}

template <typename T, typename... Ts>
void print_raw(T&& t, Ts&&... ts)
{
    cout << t;
    print_raw(ts...);
}

template <typename... Ts>
void print(Ts&&... ts)
{
    print_raw(ts...);
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

vector<path> get_src_deps(Environment const& env, path srcfile)
{
    vector<path> deps;
    string word;

    stringstream args;

    args << "-MM -MT \"\" ";
    args << env.cxxflags << " ";
    args << srcfile;

    exec_stream_t es;
    es.set_wait_timeout(exec_stream_t::s_all, 60000);
    es.start(env.cxx, args.str());

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
                print("  Missing dep: ", f);
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

CommandResult build_obj(Environment const& env, path f)
{
    stringstream args_ss;

    path objfile = env.obj/f;
    objfile.replace_extension(".o");
    objfile = normalize(objfile);

    path srcfile = normalize(env.src/f);

    args_ss << env.cxxflags << " ";
    args_ss << "-c " << srcfile << " ";
    args_ss << "-o " << objfile << " ";

    string args = args_ss.str();

    //print("Building object ", objfile);
    //print("  Command: ", env.cxx, " ", args);
    //print();

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

    //print("Building executable ", normalize(exe));
    //print("  Command: ", env.cxx, " ", args);
    //print();

    return run_command(env.cxx, args);
}

int main(int argc, char* argv[])
{
    Environment env;

    // CREATE DIRECTORIES

    create_directories(env.dep);
    create_directories(env.obj);

    auto src_files = [&]
    {
        auto _ = push_dir(env.src);
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

            make_it(env.dep);
            make_it(env.obj);
        }
    }

    // GENERATE MISSING/OUTDATED DEP FILES

    string src_exts[] = {".cpp", ".cxx", ".cc"};

    auto is_impl_file = [&](path const& srcfile)
    {
        auto b = begin(src_exts);
        auto e = end(src_exts);
        auto i = find(b, e, srcfile.extension().string());
        return (i != e);
    };

    vector<path> srcs;
    vector<path> objs;
    vector<path> objs_rebuild;

    for (auto&& ent : src_files)
    {
        if (!is_directory(ent.status()))
        {
            if (is_impl_file(ent.path()))
            {
                //print("Found source file ", ent);
                srcs.emplace_back(ent.path());

                auto dep_data = process_dep_file(env, ent.path());
                path objfile = env.obj/ent.path();
                objfile.replace_extension(".o");
                objfile = normalize(objfile);

                objs.push_back(objfile);

                bool obj_exists = exists(objfile);
                time_t obj_time = (obj_exists?last_write_time(objfile):time_t{});
                bool obj_outdated = (obj_exists?dep_data.newest>obj_time:true);

                bool obj_rebuild = false;

                if (!obj_rebuild && dep_data.missing_dep)
                {
                    print("Missing dependency for ", objfile);

                    print("BUILD FAILED");
                    return 1;
                }

                if (!obj_rebuild && !obj_exists)
                {
                    obj_rebuild = true;
                    //print("  Object missing.");
                }

                if (!obj_rebuild && obj_outdated)
                {
                    obj_rebuild = true;
                    //print("  Object out-of-date.");
                }

                if (!obj_exists || obj_outdated)
                {
                    objs_rebuild.emplace_back(ent.path());
                }
            }
        }
    }

    print("Found ", srcs.size(), " source files.");

    // BUILD OBJS

    list<CommandResult> results;
    vector<unsigned> result_flags;
    condition_variable results_ping;
    int num_objs = objs_rebuild.size();
    int objs_done = 0;
    mutex results_mtx;

    string progress_str;
    unsigned progress_str_cap;

    auto update_progress = [&]
    {
        if (!result_flags.empty())
            progress_str[1+3*result_flags.back()] = '*';
        int percent = objs_done*100/num_objs;
        print_raw("\r", progress_str, " (", percent, "%)");
    };

    using Iter = decltype(begin(objs_rebuild));

    auto builder = [&](Iter b, Iter e, unsigned c)
    {
        {
            unique_lock<mutex> _ (results_mtx);
        }

        for (; b!=e; ++b)
        {
            {
                unique_lock<mutex> _ (results_mtx);
            }

            list<CommandResult> tmp;
            tmp.emplace_back(build_obj(env, *b));

            {
                unique_lock<mutex> _ (results_mtx);
                results.splice(end(results), tmp);
                ++objs_done;
                update_progress();
            }
        }

        {
            unique_lock<mutex> _ (results_mtx);
            result_flags.emplace_back(c);
            update_progress();
            if (result_flags.size() == progress_str_cap)
                results_ping.notify_all();
        }
    };

    if (!objs_rebuild.empty())
    {
        print("Building ", objs_rebuild.size(), " objects...");

        auto sz = objs_rebuild.size();
        unsigned cores = max(thread::hardware_concurrency(), 1U);
        if (cores<1) cores = 1;
        auto workload = sz/cores;
        auto spare = sz%cores;

        vector<future<void>> threads;

        result_flags.reserve(cores);

        auto b = begin(objs_rebuild);
        decltype(sz) start = 0;

        {
            unique_lock<mutex> lck (results_mtx);

            progress_str_cap = cores;

            for (unsigned c=0; c<cores; ++c)
            {
                auto stop = start+workload;
                if (c<spare) stop+=1;
                threads.emplace_back(async(launch::async, builder, b+start, b+stop, c));
                start = stop;
                progress_str += "[ ]";
            }

            print_raw("\r", progress_str);
            while (result_flags.empty())
                results_ping.wait(lck);
            print();
        }

        for (auto&& f : threads)
            f.get();

        bool success = true;

        for (auto&& res : results)
        {
            if (!res.success)
                success = false;
            cerr << endl << res.err << endl;
        }

        if (!success)
        {
            print("BUILD FAILED");
            return 1;
        }
    }
    else
    {
        print("All objects are up-to-date!");
        print();
    }

    // BUILD EXECUTABLE

    path exe = env.bin/"a.respite";
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
        print("Building executable ", exe, "...");
        auto res = build_exe(env, exe, objs);
        if (!res.success)
        {
            cerr << endl << res.err << endl;
            print("BUILD FAILED");
            return 1;
        }
    }

    print("BUILD SUCCESS");
}
