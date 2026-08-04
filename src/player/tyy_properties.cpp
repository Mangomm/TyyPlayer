#include "tyy_properties.h"

#include <cstdlib>
#include <strings.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

using namespace TyyPlayer;

Properties::Properties()
{
}

Properties::Properties(const Properties &other)
    : _properties(other._properties)
{
}

Properties::~Properties()
{
}

Properties& Properties::operator=(const Properties &other)
{
    if (this == &other) {
        return *this;
    }

    _properties = other._properties;
    return *this;
}

bool Properties::has_property(const std::string &key) const
{
    return _properties.find(key) != _properties.end();
}

void Properties::set_property(const char* key, int intval)
{
    set_property(std::string(key), std::to_string(intval));
}

void Properties::set_property(const char* key, uint32_t val)
{
    set_property(std::string(key), std::to_string(val));
}

void Properties::set_property(const char* key, uint64_t val)
{
    set_property(std::string(key), std::to_string(val));
}

void Properties::set_property(const char* key, const char* val)
{
    set_property(std::string(key), std::string(val));
}

void Properties::set_property(const std::string &key, const std::string &val)
{
    _properties[key] = val;
}

const char* Properties::get_property(const char* key) const
{
    return get_property(key, "");
}

std::string Properties::get_property(const char* key, const std::string default_value) const
{
    return get_property(std::string(key), default_value);
}

std::string Properties::get_property(const std::string &key, const std::string default_value) const
{
    std::map<std::string, std::string>::const_iterator it = _properties.find(key);
    if (it == _properties.end()) {
        return default_value;
    }

    return it->second;
}

const char* Properties::get_property(const char* key, const char *default_value) const
{
    std::map<std::string, std::string>::const_iterator it = _properties.find(std::string(key));
    if (it == _properties.end()) {
        return default_value;
    }

    return it->second.c_str();
}

const char* Properties::get_property(const std::string &key, char *default_value) const
{
    std::map<std::string, std::string>::const_iterator it = _properties.find(key);
    if (it == _properties.end()) {
        return default_value;
    }

    return it->second.c_str();
}

int Properties::get_property(const char* key, int default_value) const
{
    return get_property(std::string(key), default_value);
}

int Properties::get_property(const std::string &key, int default_value) const
{
    std::map<std::string, std::string>::const_iterator it = _properties.find(key);
    if (it == _properties.end()) {
        return default_value;
    }

    return atoi(it->second.c_str());
}

uint64_t Properties::get_property(const char* key, uint64_t default_value) const
{
    return get_property(std::string(key), default_value);
}

uint64_t Properties::get_property(const std::string &key, uint64_t default_value) const
{
    std::map<std::string, std::string>::const_iterator it = _properties.find(key);
    if (it == _properties.end()) {
        return default_value;
    }

    return atoll(it->second.c_str());
}

bool Properties::get_property(const char* key, bool default_value) const
{
    return get_property(std::string(key), default_value);
}

bool Properties::get_property(const std::string &key, bool default_value) const
{
    std::map<std::string, std::string>::const_iterator it = _properties.find(key);
    if (it == _properties.end()) {
        return default_value;
    }

    const char *val = it->second.c_str();
    if (strcasecmp(val, "yes") == 0) {
        return true;
    }
    if (strcasecmp(val, "true") == 0) {
        return true;
    }

    return atoi(val) != 0;
}
