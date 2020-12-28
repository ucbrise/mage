/*
 * Copyright (C) 2020 Sam Kumar <samkumar@cs.berkeley.edu>
 * Copyright (C) 2020 University of California, Berkeley
 * All rights reserved.
 *
 * This file is part of MAGE.
 *
 * MAGE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * MAGE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MAGE.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef MAGE_UTIL_CONFIG_HPP_
#define MAGE_UTIL_CONFIG_HPP_

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace mage::util {
    class ConfigException : public std::runtime_error {
    public:
        ConfigException(const std::string& what) : std::runtime_error(what) {
        }
    };

    class ConfigDoesNotExistException : public ConfigException {
    public:
        ConfigDoesNotExistException(const std::string& what) : ConfigException(what) {
        }
    };

    class ConfigBadTypeException : public ConfigException {
    public:
        ConfigBadTypeException(const std::string& what) : ConfigException(what) {
        }
    };

    class ConfigIncompatibleKeysException : public ConfigException {
    public:
        ConfigIncompatibleKeysException(const std::string& what) : ConfigException(what) {
        }
    };

    class ConfigInvalidAccessException : public ConfigException {
    public:
        ConfigInvalidAccessException(const std::string& what) : ConfigException(what) {
        }
    };

    class ConfigNode;
    class ConfigMapNode;
    class ConfigListNode;

    class ConfigValue {
        friend class ConfigNode;

    public:
        ConfigValue(const ConfigNode* parent, const std::string& field);
        ConfigValue(const ConfigNode* parent, const std::string& field, const YAML::Node& n);

        const std::string& as_string() const;
        std::int64_t as_int() const;
        const std::unordered_map<std::string, std::unique_ptr<ConfigValue>>& as_map() const;
        const std::vector<std::unique_ptr<ConfigValue>>& as_list() const;

        const ConfigValue* get(const std::string& key) const;
        const ConfigValue* get(const std::vector<std::string>& key_set) const;
        const ConfigValue* get(std::size_t index) const;
        const ConfigValue& operator [](const std::string& key) const;
        const ConfigValue& operator [](const std::vector<std::string>& key_set) const;
        const ConfigValue& operator [](std::size_t index) const;
        std::size_t get_size() const;

        std::string get_field_name() const;
        std::string get_field_path() const;

    protected:
        const ConfigNode* as_node() const;
        const ConfigListNode& as_list_node() const;

        void initialize(const YAML::Node& n);

    private:
        const ConfigNode* parent_node;
        std::string field_name;
        std::variant<std::string, std::unique_ptr<ConfigNode>> data;
    };

    class ConfigNode {
        friend class ConfigValue;

    public:
        ConfigNode(ConfigValue* parent);
        virtual ~ConfigNode();
        virtual const ConfigValue* get(const std::string& key) const;
        virtual std::size_t get_size() const = 0;

        std::string get_field_path() const;

    protected:
        ConfigValue* parent_value;
    };

    class ConfigMapNode : public ConfigNode {
        friend class ConfigValue;

    public:
        ConfigMapNode(ConfigValue* parent);
        ConfigMapNode(ConfigValue* parent, const YAML::Node& n);
        void from_yaml(const YAML::Node& n);
        const ConfigValue* get(const std::string& key) const override;
        std::size_t get_size() const override;

    private:
        std::unordered_map<std::string, std::unique_ptr<ConfigValue>> fields;
    };

    class ConfigListNode : public ConfigNode {
        friend class ConfigValue;

    public:
        ConfigListNode(ConfigValue* parent);
        ConfigListNode(ConfigValue* parent, const YAML::Node& n);
        void from_yaml(const YAML::Node& n);
        const ConfigValue* get(std::size_t index) const;
        std::size_t get_size() const override;

    private:
        std::vector<std::unique_ptr<ConfigValue>> elements;
    };

    class Configuration : public ConfigValue {
    public:
        Configuration();
        Configuration(const std::string& yaml_file);
        void load_yaml_file(const std::string& yaml_file);
    };
}

#endif
