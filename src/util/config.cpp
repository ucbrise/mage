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

#include "util/config.hpp"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <yaml-cpp/yaml.h>

namespace mage::util {
    ConfigValue::ConfigValue(const ConfigNode* parent, const std::string& field) : parent_node(parent), field_name(field) {
    }

    ConfigValue::ConfigValue(const ConfigNode* parent, const std::string& field, const YAML::Node& n) : ConfigValue(parent, field) {
        this->initialize(n);
    }

    void ConfigValue::initialize(const YAML::Node& n) {
        switch (n.Type()) {
        case YAML::NodeType::Scalar:
            this->data = n.Scalar();
            break;
        case YAML::NodeType::Sequence:
            this->data = std::make_unique<ConfigListNode>(this, n);
            break;
        case YAML::NodeType::Map:
            this->data = std::make_unique<ConfigMapNode>(this, n);
            break;
        case YAML::NodeType::Undefined:
        case YAML::NodeType::Null:
        default:
            std::abort();
        }
    }

    const std::string& ConfigValue::as_string() const {
        try {
            return std::get<std::string>(this->data);
        } catch (const std::bad_variant_access& bva) {
            throw ConfigBadTypeException("Field " + this->get_field_path() + ": accessed as a string, but is not a scalar");
        }
    }

    std::int64_t ConfigValue::as_int() const {
        const std::string& value = this->as_string();
        try {
            return std::stoll(value, nullptr);
        } catch (const std::invalid_argument& ia) {
            throw ConfigBadTypeException("Field " + this->get_field_path() + ": could not be parsed as an int (" + value + ")");
        }
    }

    const ConfigNode* ConfigValue::as_node() const {
        try {
            return std::get<std::unique_ptr<ConfigNode>>(this->data).get();
        } catch (const std::bad_variant_access& bva) {
            throw ConfigBadTypeException("Field " + this->get_field_path() + ": accessed as a node, but is not a node");
        }
    }

    const std::unordered_map<std::string, std::unique_ptr<ConfigValue>>& ConfigValue::as_map() const {
        const ConfigNode* node = this->as_node();
        const ConfigMapNode* map = dynamic_cast<const ConfigMapNode*>(node);
        if (map == nullptr) {
            throw ConfigBadTypeException("Field " + this->get_field_path() + ": accessed as a map, but is not a map");
        }
        return map->fields;
    }

    const ConfigListNode& ConfigValue::as_list_node() const {
        const ConfigNode* node = this->as_node();
        const ConfigListNode* list = dynamic_cast<const ConfigListNode*>(node);
        if (list == nullptr) {
            throw ConfigBadTypeException("Field " + this->get_field_path() + ": accessed as a list, but is not a list");
        }
        return *list;
    }

    const std::vector<std::unique_ptr<ConfigValue>>& ConfigValue::as_list() const {
        const ConfigListNode& list = this->as_list_node();
        return list.elements;
    }

    const ConfigValue* ConfigValue::get(const std::string& key) const {
        const ConfigNode* node = this->as_node();
        return node->get(key);
    }

    const ConfigValue* ConfigValue::get(const std::vector<std::string>& key_set) const {
        const ConfigNode* node = this->as_node();
        const ConfigValue* result = nullptr;
        const std::string* result_key = nullptr;
        for (const std::string& key : key_set) {
            const ConfigValue* key_result = node->get(key);
            if (key_result != nullptr) {
                if (result != nullptr) {
                    throw ConfigIncompatibleKeysException("In " + this->get_field_path() + ": keys \"" + *result_key + "\" and \"" + key + "\" are both present");
                }
                result = key_result;
                result_key = &key;
            }
        }
        return result;
    }

    const ConfigValue* ConfigValue::get(const std::size_t index) const {
        const ConfigListNode& node = this->as_list_node();
        return node.get(index);
    }

    const ConfigValue& ConfigValue::operator [](const std::string& key) const {
        const ConfigValue* result = this->get(key);
        if (result == nullptr) {
            throw ConfigDoesNotExistException("In " + this->get_field_path() + ": key \"" + key + "\" expected but does not exist");
        }
        return *result;
    }

    const ConfigValue& ConfigValue::operator [](const std::vector<std::string>& key_set) const {
        const ConfigValue* result = this->get(key_set);
        if (result == nullptr) {
            if (key_set.size() == 0) {
                throw ConfigInvalidAccessException("At " + this->get_field_path() + ": attempted to access with empty key set");
            }
            if (key_set.size() == 1) {
                throw ConfigDoesNotExistException("In " + this->get_field_path() + ": key \"" + key_set[0] + "\" expected but does not exist");
            }
            std::ostringstream message;
            message << "In " << this->get_field_path() << ": expected one of the keys ";
            for (std::size_t i = 0; i != key_set.size(); i++) {
                if (i + 1 != key_set.size()) {
                    message << "\"" << key_set[i] << "\"";
                    if (key_set.size() == 2) {
                        message << " ";
                    } else {
                        message << ", ";
                    }
                } else {
                    message << "or \"" << key_set[i] << "\" but none exist";
                }
            }
            throw ConfigDoesNotExistException(message.str());
        }
        return *result;
    }

    const ConfigValue& ConfigValue::operator [](const std::size_t index) const {
        const ConfigValue* result = this->get(index);
        if (result == nullptr) {
            throw ConfigDoesNotExistException("In " + this->get_field_path() + ": index " + std::to_string(index) + " does not exist");
        }
        return *result;
    }

    std::size_t ConfigValue::get_size() const {
        return this->as_node()->get_size();
    }

    std::string ConfigValue::get_field_name() const {
        return this->field_name;
    }

    std::string ConfigValue::get_field_path() const {
        if (this->parent_node != nullptr) {
            return this->parent_node->get_field_path() + this->field_name;
        }
        return this->field_name;
    }

    ConfigNode::ConfigNode(ConfigValue* parent) : parent_value(parent) {
    }

    ConfigNode::~ConfigNode() {
    }

    const ConfigValue* ConfigNode::get(const std::string& key) const {
        if (this->parent_value->parent_node != nullptr) {
            return this->parent_value->parent_node->get(key);
        }
        return nullptr;
    }

    std::string ConfigNode::get_field_path() const {
        return this->parent_value->get_field_path() + "/";
    }

    ConfigMapNode::ConfigMapNode(ConfigValue* parent) : ConfigNode(parent) {
    }

    ConfigMapNode::ConfigMapNode(ConfigValue* parent, const YAML::Node& node) : ConfigNode(parent) {
        this->from_yaml(node);
    }

    void ConfigMapNode::from_yaml(const YAML::Node& node) {
        if (node.IsMap()) {
            for (YAML::const_iterator it = node.begin(); it != node.end(); it++) {
                std::string key = it->first.as<std::string>();
                auto result = this->fields.emplace(std::piecewise_construct, std::tuple(key), std::tuple(std::make_unique<ConfigValue>(this, key, it->second)));
                if (!result.second) {
                    std::cerr << "Found duplicate key " << key << std::endl;
                    std::abort();
                }
            }
        } else {
            std::cerr << "YAML node has incorrect type" << std::endl;
            std::abort();
        }
    }

    const ConfigValue* ConfigMapNode::get(const std::string& key) const {
        auto iter = this->fields.find(key);
        if (iter != this->fields.end()) {
            return iter->second.get();
        }
        if (this->parent_value->get_field_name() == key) {
            return nullptr;
        }
        return this->ConfigNode::get(key);
    }

    std::size_t ConfigMapNode::get_size() const {
        return this->fields.size();
    }

    ConfigListNode::ConfigListNode(ConfigValue* parent) : ConfigNode(parent) {
    }

    ConfigListNode::ConfigListNode(ConfigValue* parent, const YAML::Node& node) : ConfigNode(parent) {
        this->from_yaml(node);
    }

    std::size_t ConfigListNode::get_size() const {
        return this->elements.size();
    }

    void ConfigListNode::from_yaml(const YAML::Node& node) {
        if (node.IsSequence()) {
            for (std::size_t i = 0; i != node.size(); i++) {
                std::string key = std::to_string(i);
                this->elements.emplace_back(std::make_unique<ConfigValue>(this, key, node[i]));
            }
        } else {
            std::cerr << "YAML node has incorrect type" << std::endl;
            std::abort();
        }
    }

    const ConfigValue* ConfigListNode::get(const std::size_t index) const {
        if (index < this->elements.size()) {
            return this->elements[index].get();
        }
        return nullptr;
    }

    Configuration::Configuration() : ConfigValue(nullptr, "") {
    }

    Configuration::Configuration(const std::string& yaml_file) : Configuration() {
        this->load_yaml_file(yaml_file);
    }

    void Configuration::load_yaml_file(const std::string& yaml_file) {
        YAML::Node root;
        try {
            root = YAML::LoadFile(yaml_file);
        } catch (const YAML::Exception& ye) {
            throw ConfigException(yaml_file + ": " + ye.msg);
        }
        this->initialize(root);
    }
}
