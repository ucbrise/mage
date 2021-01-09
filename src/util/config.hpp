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

/**
 * @file util/config.hpp
 * @brief Tools for parsing and interpreting MAGE's configuration file.
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
    /**
     * @brief Thrown when an unresolvable error is encountered in parsing or
     * interpreting the configuration file.
     */
    class ConfigException : public std::runtime_error {
    public:
        ConfigException(const std::string& what) : std::runtime_error(what) {
        }
    };

    /**
     * @brief Thrown when an item is expected to exist in the configuration
     * file but is not found in the configuration file.
     */
    class ConfigDoesNotExistException : public ConfigException {
    public:
        ConfigDoesNotExistException(const std::string& what) : ConfigException(what) {
        }
    };

    /**
     * @brief Thrown when an item is found in the configuration file but does
     * not have the expected type (e.g., an item is expected to be a string
     * but is actually a list.
     */
    class ConfigBadTypeException : public ConfigException {
    public:
        ConfigBadTypeException(const std::string& what) : ConfigException(what) {
        }
    };

    /**
     * @brief Thrown when two keys which cannot both be present in the
     * configuration file (e.g., because they are synonyms) are found.
     */
    class ConfigIncompatibleKeysException : public ConfigException {
    public:
        ConfigIncompatibleKeysException(const std::string& what) : ConfigException(what) {
        }
    };

    /**
     * @brief Thrown when one attempts to access the configuration file in an
     * ill-formed way.
     */
    class ConfigInvalidAccessException : public ConfigException {
    public:
        ConfigInvalidAccessException(const std::string& what) : ConfigException(what) {
        }
    };

    class ConfigNode;
    class ConfigMapNode;
    class ConfigListNode;

    /**
     * @brief Represents an item in the configuration file, which may be
     * primitive value (e.g., a string or integer) or a structure containing
     * more items (e.g., a list or map).
     */
    class ConfigValue {
        friend class ConfigNode;

    public:
        ConfigValue(const ConfigNode* parent, const std::string& field);
        ConfigValue(const ConfigNode* parent, const std::string& field, const YAML::Node& n);

        /**
         * @brief Returns the item represented by this ConfigValue as a string.
         *
         * @exception ConfigBadTypeException This ConfigValue represents a
         * structured item (e.g., list or map).
         *
         * @return A reference to the underlying string data for the item.
         */
        const std::string& as_string() const;

        /**
         * @brief Returns the item represented by this ConfigValue as an
         * integer.
         *
         * @exception ConfigBadTypeException This ConfigValue represents a
         * structured item (e.g., list or map) or if parsing the item as an
         * integer fails.
         *
         * @return The item in integer form.
         */
        std::int64_t as_int() const;

        /**
         * @brief Returns the item represented by this ConfigValue as a map.
         *
         * @exception ConfigBadTypeException This ConfigValue does not
         * represent a map.
         *
         * @return A reference to the underlying map data for the item.
         */
        const std::unordered_map<std::string, std::unique_ptr<ConfigValue>>& as_map() const;

        /**
         * @brief Returns the item represented by this ConfigValue as a list.
         *
         * @exception ConfigBadTypeException This ConfigValue does not
         * represent a list.
         *
         * @return A reference to the underlying list data for the item.
         */
        const std::vector<std::unique_ptr<ConfigValue>>& as_list() const;

        /**
         * @brief Looks up the child of this item using the provided key.
         * If no child of this item with a matching key is found, lookup
         * continues in the parent of this node, and so on until the key is
         * looked up in the root node of the configuration file.
         *
         * @exception ConfigBadTypeException This ConfigValue represents a
         * non-structured item (e.g., string or integer).
         *
         * @param key The key used for lookup.
         * @return A pointer to the ConfigValue for the desired item, or
         * a null pointer if lookup fails.
         */
        const ConfigValue* get(const std::string& key) const;

        /**
         * @brief Looks up a child of this item using the provided keys,
         * using the same procedure as the single-key version of this
         * function.
         *
         * @exception ConfigBadTypeException This ConfigValue represents a
         * non-structured item (e.g., string or integer).
         * @exception ConfigIncompatibleKeysException Multiple keys in the
         * provided key set result in successful lookups.
         *
         * @param key_set The keys used for lookup.
         * @return A pointer to the ConfigValue for the desired item, or
         * a null pointer if lookup fails.
         */
        const ConfigValue* get(const std::vector<std::string>& key_set) const;

        /**
         * @brief Looks up the child of this item using the provided index.
         * If no child of this item with a matching index is found, lookup
         * terminates without consulting this item's ancestors.
         *
         * @exception ConfigBadTypeException This ConfigValue represents an
         * item that is not a list.
         *
         * @param index The index used for lookup.
         * @return A pointer to the ConfigValue for the desired item, or
         * a null pointer if lookup fails.
         */
        const ConfigValue* get(std::size_t index) const;

        /**
         * @brief Looks up the child of this item using the provided key.
         * If no child of this item with a matching key is found, lookup
         * continues in the parent of this node, and so on until the key is
         * looked up in the root node of the configuration file.
         *
         * @exception ConfigBadTypeException This ConfigValue represents a
         * non-structured item (e.g., string or integer).
         * @exception ConfigDoesNotExistException Lookup fails.
         *
         * @param key The key used for lookup.
         * @return A reference to the ConfigValue for the desired item.
         */
        const ConfigValue& operator [](const std::string& key) const;

        /**
         * @brief Looks up a child of this item using the provided keys,
         * using the same procedure as the single-key version of this
         * function.
         *
         * @exception ConfigBadTypeException This ConfigValue represents a
         * non-structured item (e.g., string or integer).
         * @exception ConfigIncompatibleKeysException Multiple keys in the
         * provided key set result in successful lookups.
         * @exception ConfigDoesNotExistException Lookup fails for all
         * provided keys.
         * @exception ConfigInvalidAccessException The provided key set is
         * empty.
         *
         * @param key_set The keys used for lookup.
         * @return A reference to the ConfigValue for the desired item.
         */
        const ConfigValue& operator [](const std::vector<std::string>& key_set) const;

        /**
         * @brief Looks up the child of this item using the provided index.
         * If no child of this item with a matching index is found, lookup
         * terminates without consulting this item's ancestors.
         *
         * @exception ConfigBadTypeException This ConfigValue represents an
         * item that is not a list.
         * @exception ConfigDoesNotExistException Lookup fails.
         *
         * @param index The index used for lookup.
         * @return A reference to the ConfigValue for the desired item.
         */
        const ConfigValue& operator [](std::size_t index) const;

        /**
         * @brief Returns the number of children of this item in the
         * configuration file.
         *
         * @exception ConfigBadTypeException This ConfigValue represents a
         * non-structured item (e.g., a string or integer).
         *
         * @return The number of children of this item.
         */
        std::size_t get_size() const;

        /**
         * @brief Provides the name corresponding to this item in the
         * configuration file.
         *
         * @return The name corresponding to this item in the configuration
         * file.
         */
        std::string get_field_name() const;

        /**
         * @brief Provides the full path corresponding to this item in the
         * configuration file.
         *
         * The full path consists of the name of this item, the name of its
         * parent, and so on until the root of the configuration file,
         * concatentated with forward slashes in order from root to this item.
         *
         * @return The full path corresponding to this item in the
         * configuration file.
         */
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

    /**
     * @brief Represents a structured item (e.g., a list or a map) in the
     * configuration file that may contain other items.
     *
     * Users of the configuration file tools will normally not have to use this
     * class directly.
     */
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

    /**
     * @brief Represents a map in the configuration file containing other
     * items.
     *
     * Users of the configuration file tools will normally not have to use this
     * class directly.
     */
    class ConfigMapNode : public ConfigNode {
        friend class ConfigValue;

    public:
        ConfigMapNode(ConfigValue* parent, const YAML::Node& n);
        ConfigMapNode(ConfigValue* parent);
        void from_yaml(const YAML::Node& n);
        const ConfigValue* get(const std::string& key) const override;
        std::size_t get_size() const override;

    private:
        std::unordered_map<std::string, std::unique_ptr<ConfigValue>> fields;
    };

    /**
     * @brief Represents a list in the configuration file containing other
     * items.
     *
     * Users of the configuration file tools will normally not have to use this
     * class directly.
     */
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

    /**
     * @brief A ConfigValue representing the root of a configuration file.
     */
    class Configuration : public ConfigValue {
    public:
        /**
         * @brief Creates an invalid representation of an configuration file.
         *
         * Before using a Configuration created with this constructor, one
         * should call load_yaml_file().
         */
        Configuration();

        /**
         * @brief Parses the configuration file with the specified file name
         * and creates a Configuration representing it.
         *
         * @param yaml_file The file name of the configuration file.
         */
        Configuration(const std::string& yaml_file);

        /**
         * @brief Parses the configuration file with the specified file name
         * and initializes this Configuration object with its contents.
         */
        void load_yaml_file(const std::string& yaml_file);
    };
}

#endif
