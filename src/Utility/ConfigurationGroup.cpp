/*
    Copyright © 2007, 2008, 2009, 2010, 2011, 2012
              Vladimír Vondruš <mosra@centrum.cz>

    This file is part of Corrade.

    Corrade is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License version 3
    only, as published by the Free Software Foundation.

    Corrade is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Lesser General Public License version 3 for more details.
*/

#include "ConfigurationGroup.h"

#include "Configuration.h"
#include "Debug.h"

using namespace std;

namespace Corrade { namespace Utility {

ConfigurationGroup::ConfigurationGroup(const ConfigurationGroup& other): items(other.items), _groups(other._groups) {
    /* Deep copy groups */
    for(vector<Group>::iterator it = _groups.begin(); it != _groups.end(); ++it)
        it->group = new ConfigurationGroup(*it->group);
}

ConfigurationGroup& ConfigurationGroup::operator=(const ConfigurationGroup& other) {
    if(&other == this)
        return *this;

    /* Delete current groups */
    for(vector<Group>::iterator it = _groups.begin(); it != _groups.end(); ++it)
        delete it->group;

    items.assign(other.items.begin(), other.items.end());
    _groups.assign(other._groups.begin(), other._groups.end());

    /* Deep copy groups */
    for(vector<Group>::iterator it = _groups.begin(); it != _groups.end(); ++it)
        it->group = new ConfigurationGroup(*it->group);

    return *this;
}

ConfigurationGroup::~ConfigurationGroup() {
    for(vector<Group>::iterator it = _groups.begin(); it != _groups.end(); ++it)
        delete it->group;
}

ConfigurationGroup* ConfigurationGroup::group(const string& name, unsigned int number) {
    unsigned int foundNumber = 0;
    for(vector<Group>::iterator it = _groups.begin(); it != _groups.end(); ++it) {
        if(it->name == name && foundNumber++ == number)
            return it->group;
    }

    /* Automatic group creation is enabled and user wants first group,
        try to create new group */
    if((configuration->flags & Configuration::AutoCreateGroups) && number == 0) return addGroup(name);

    return nullptr;
}

const ConfigurationGroup* ConfigurationGroup::group(const string& name, unsigned int number) const {
    unsigned int foundNumber = 0;
    for(vector<Group>::const_iterator it = _groups.begin(); it != _groups.end(); ++it) {
        if(it->name == name && foundNumber++ == number)
            return it->group;
    }

    return nullptr;
}

vector<ConfigurationGroup*> ConfigurationGroup::groups(const string& name) {
    vector<ConfigurationGroup*> found;

    for(vector<Group>::iterator it = _groups.begin(); it != _groups.end(); ++it)
        if(name.empty() || it->name == name) found.push_back(it->group);

    return found;
}

vector<const ConfigurationGroup*> ConfigurationGroup::groups(const string& name) const {
    vector<const ConfigurationGroup*> found;

    for(vector<Group>::const_iterator it = _groups.begin(); it != _groups.end(); ++it)
        if(name.empty() || it->name == name) found.push_back(it->group);

    return found;
}

bool ConfigurationGroup::addGroup(const string& name, ConfigurationGroup* group) {
    if(configuration->flags & Configuration::ReadOnly || !(configuration->flags & Configuration::IsValid))
        return false;

    /* Set configuration pointer to actual */
    group->configuration = configuration;

    /* Name must not be empty and must not contain slash character */
    if(name.empty() || name.find('/') != string::npos) {
        Error() << "Slash in group name!";
        return false;
    }

    /* Check for unique groups */
    if(configuration->flags & Configuration::UniqueGroups) {
        for(vector<Group>::const_iterator it = _groups.begin(); it != _groups.end(); ++it)
            if(it->name == name) return false;
    }

    configuration->flags |= Configuration::Changed;

    Group g;
    g.name = name;
    g.group = group;
    _groups.push_back(g);
    return true;
}

ConfigurationGroup* ConfigurationGroup::addGroup(const std::string& name) {
    ConfigurationGroup* group = new ConfigurationGroup(configuration);
    if(!addGroup(name, group)) {
        delete group;
        group = nullptr;
    }
    return group;
}

bool ConfigurationGroup::removeGroup(const std::string& name, unsigned int number) {
    if(configuration->flags & Configuration::ReadOnly || !(configuration->flags & Configuration::IsValid)) return false;

    /* Find group with given number and name */
    unsigned int foundNumber = 0;
    for(vector<Group>::iterator it = _groups.begin(); it != _groups.end(); ++it) {
        if(it->name == name && foundNumber++ == number) {
            delete it->group;
            _groups.erase(it);
            configuration->flags |= Configuration::Changed;
            return true;
        }
    }

    return false;
}

bool ConfigurationGroup::removeGroup(ConfigurationGroup* group) {
    if(configuration->flags & Configuration::ReadOnly || !(configuration->flags & Configuration::IsValid)) return false;

    for(vector<Group>::iterator it = _groups.begin(); it != _groups.end(); ++it) {
        if(it->group == group) {
            delete it->group;
            _groups.erase(it);
            configuration->flags |= Configuration::Changed;
            return true;
        }
    }

    return false;
}

bool ConfigurationGroup::removeAllGroups(const std::string& name) {
    if(configuration->flags & Configuration::ReadOnly || !(configuration->flags & Configuration::IsValid)) return false;

    for(int i = _groups.size()-1; i >= 0; --i) {
        if(_groups[i].name != name) continue;
        delete (_groups.begin()+i)->group;
        _groups.erase(_groups.begin()+i);
    }

    configuration->flags |= Configuration::Changed;
    return true;
}

unsigned int ConfigurationGroup::keyCount(const string& key) const {
    unsigned int count = 0;
    for(vector<Item>::const_iterator it = items.begin(); it != items.end(); ++it)
        if(it->key == key) count++;

    return count;
}

bool ConfigurationGroup::keyExists(const std::string& key) const {
    for(vector<Item>::const_iterator it = items.begin(); it != items.end(); ++it)
        if(it->key == key) return true;

    return false;
}

vector<string> ConfigurationGroup::valuesInternal(const string& key, int) const {
    vector<string> found;

    for(vector<Item>::const_iterator it = items.begin(); it != items.end(); ++it)
        if(it->key == key) found.push_back(it->value);

    return found;
}

bool ConfigurationGroup::setValueInternal(const string& key, const string& value, unsigned int number, int) {
    if(configuration->flags & Configuration::ReadOnly || !(configuration->flags & Configuration::IsValid))
        return false;

    /* Key cannot be empty => this would change comments / empty lines */
    if(key.empty()) return false;

    unsigned int foundNumber = 0;
    for(vector<Item>::iterator it = items.begin(); it != items.end(); ++it) {
        if(it->key == key && foundNumber++ == number) {
            it->value = value;
            configuration->flags |= Configuration::Changed;
            return true;
        }
    }

    /* No value with that name was found, add new */
    Item i;
    i.key = key;
    i.value = value;
    items.push_back(i);

    configuration->flags |= Configuration::Changed;
    return true;
}

bool ConfigurationGroup::addValueInternal(const string& key, const string& value, int) {
    if(configuration->flags & Configuration::ReadOnly || !(configuration->flags & Configuration::IsValid))
        return false;

    /* Key cannot be empty => empty keys are treated as comments / empty lines */
    if(key.empty()) return false;

    /* Check for unique keys */
    if(configuration->flags & Configuration::UniqueKeys) {
        for(vector<Item>::const_iterator it = items.begin(); it != items.end(); ++it)
            if(it->key == key) return false;
    }

    Item i;
    i.key = key;
    i.value = value;
    items.push_back(i);

    configuration->flags |= Configuration::Changed;
    return true;
}

bool ConfigurationGroup::valueInternal(const string& key, string* value, unsigned int number, int flags) {
    const ConfigurationGroup* c = this;
    if(c->value(key, value, number, flags)) return true;

    /* Automatic key/value pair creation is enabled and user wants first key,
        try to create new key/value pair */
    if((configuration->flags & Configuration::AutoCreateKeys) && number == 0)
        return setValue<string>(key, *value, number, flags);

    return false;
}

bool ConfigurationGroup::valueInternal(const string& key, string* value, unsigned int number, int) const {
    unsigned int foundNumber = 0;
    for(vector<Item>::const_iterator it = items.begin(); it != items.end(); ++it) {
        if(it->key == key) {
            if(foundNumber++ == number) {
                *value = it->value;
                return true;
            }
        }
    }

    return false;
}

bool ConfigurationGroup::removeValue(const string& key, unsigned int number) {
    if(configuration->flags & Configuration::ReadOnly || !(configuration->flags & Configuration::IsValid))
        return false;

    /* Key cannot be empty => empty keys are treated as comments / empty lines */
    if(key.empty()) return false;

    unsigned int foundNumber = 0;
    for(vector<Item>::iterator it = items.begin(); it != items.end(); ++it) {
        if(it->key == key && foundNumber++ == number) {
            items.erase(it);
            configuration->flags |= Configuration::Changed;
            return true;
        }
    }

    return false;
}

bool ConfigurationGroup::removeAllValues(const std::string& key) {
    if(configuration->flags & Configuration::ReadOnly || !(configuration->flags & Configuration::IsValid))
        return false;

    /** @todo Do it better & faster */
    for(int i = items.size()-1; i >= 0; --i) {
        if(items[i].key == key) items.erase(items.begin()+i);
    }

    configuration->flags |= Configuration::Changed;
    return true;
}

bool ConfigurationGroup::clear() {
    if(configuration->flags & Configuration::ReadOnly || !(configuration->flags & Configuration::IsValid))
        return false;

    items.clear();

    for(vector<Group>::iterator it = _groups.begin(); it != _groups.end(); ++it)
        delete it->group;
    _groups.clear();

    return true;
}

#ifndef DOXYGEN_GENERATING_OUTPUT

bool ConfigurationValue<bool>::fromString(const std::string& value, int) {
    if(value == "1" || value == "yes" || value == "y" || value == "true") return true;
    return false;
}

std::string ConfigurationValue<bool>::toString(const bool& value, int) {
    if(value) return "true";
    return "false";
}

#endif

}}
