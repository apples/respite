#ifndef FILESYSTEM_UTILS_HPP
#define FILESYSTEM_UTILS_HPP

#include <vector>
#include <boost/filesystem.hpp>

boost::filesystem::path normalize(boost::filesystem::path source);

std::vector<boost::filesystem::directory_entry> recursive_list(boost::filesystem::path p);

class push_dir
{
    boost::filesystem::path prev;

    public:

        push_dir(boost::filesystem::path p);
        ~push_dir();
};

#endif // FILESYSTEM_UTILS_HPP
