#ifndef DEPS_HPP
#define DEPS_HPP

#include <ctime>
#include <vector>
#include <boost/filesystem.hpp>

#include "environment.hpp"

struct DepData
{
    bool missing_dep = false;
    std::time_t newest = std::time_t{};
};

std::vector<boost::filesystem::path> get_src_deps(Environment const& env, boost::filesystem::path srcfile);

std::vector<boost::filesystem::path> read_dep_file(boost::filesystem::path depfile);

void write_dep_file(boost::filesystem::path f, std::vector<boost::filesystem::path> const& deps);

DepData process_dep_file(Environment const& env, boost::filesystem::path ent);

#endif // DEPS_HPP
