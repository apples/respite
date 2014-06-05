#include "filesystem_utils.hpp"

#include <vector>
#include <boost/filesystem.hpp>

using namespace std;
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

push_dir::push_dir(path p)
    : prev(current_path())
{
    current_path(p);
}

push_dir::~push_dir()
{
    current_path(prev);
}
