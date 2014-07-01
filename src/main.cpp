#include <algorithm>
#include <iostream>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/filesystem.hpp>

#include "build.hpp"
#include "command.hpp"
#include "deps.hpp"
#include "environment.hpp"
#include "filesystem_utils.hpp"
#include "print.hpp"

using namespace std;
using namespace boost::filesystem;

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
        if (is_directory(env.src/ent))
        {
            create_directories(env.dep/ent.path());
            create_directories(env.obj/ent.path());
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

    {
        auto sz = srcs.size();
        print("Found ", sz, " source file", (sz>1?"s":""), ".");
    }

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
        print("Building ", num_objs, " object", (num_objs>1?"s":""), "...");

        auto sz = objs_rebuild.size();
        unsigned cores = max(thread::hardware_concurrency(), 1U);
        if (cores<1) cores = 1;
        auto workload = sz/cores;
        auto spare = sz%cores;

        vector<thread> threads;

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
                threads.emplace_back(builder, b+start, b+stop, c);
                start = stop;
                progress_str += "[ ]";
            }

            print_raw("\r", progress_str, "(0%)");
            while (result_flags.empty())
                results_ping.wait(lck);
            print();
        }

        for (auto&& f : threads)
            f.join();

        bool success = true;

        for (auto&& res : results)
        {
            if (!res.success)
                success = false;
            if (!res.err.empty())
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
