#ifndef ENVIRONMENT_HPP
#define ENVIRONMENT_HPP

#include <string>
#include <boost/filesystem.hpp>

struct Environment
{
    static char const* getenv_safe(std::string const& var);

    const boost::filesystem::path base;
    const boost::filesystem::path src;
    const boost::filesystem::path dep;
    const boost::filesystem::path obj;
    const boost::filesystem::path bin;

    const std::string cxx;
    const std::string cppflags;
    const std::string cxxflags;
    const std::string ldflags;
    const std::string ldlibs;

    Environment();
};

#endif // ENVIRONMENT_HPP
