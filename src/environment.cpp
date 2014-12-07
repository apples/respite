#include "environment.hpp"

#include <cstdlib>
#include <string>
#include <map>
#include <boost/filesystem.hpp>

using namespace std;
using namespace boost::filesystem;

char const* Environment::getenv_safe(std::string const& var) // static
{
    static std::map<std::string,char const*> defaults = {
        {"CXX","g++"},
        {"CPPFLAGS",""},
        {"CXXFLAGS","-std=c++11 -Wall -O2 -g"},
        {"LDFLAGS",""},
        {"LDLIBS",""},
    };

    char const* rv = std::getenv(var.c_str());
    if (!rv) rv = defaults[var];
    return rv;
}

Environment::Environment()
    : base(current_path())
    , src("src")
    , dep(".respite/dep")
    , obj(".respite/obj")
    , bin(".")
    , cxx(getenv_safe("CXX"))
    , cppflags(getenv_safe("CPPFLAGS"))
    , cxxflags(getenv_safe("CXXFLAGS"))
    , ldflags(getenv_safe("LDFLAGS"))
    , ldlibs(getenv_safe("LDLIBS"))
{}
