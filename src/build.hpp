#ifndef BUILD_HPP
#define BUILD_HPP

#include <vector>
#include <boost/filesystem.hpp>

#include "command.hpp"
#include "environment.hpp"

CommandResult build_obj(Environment const& env, boost::filesystem::path f);

CommandResult build_exe(Environment const& env, boost::filesystem::path exe, std::vector<boost::filesystem::path> const& objs);

#endif // BUILD_HPP
