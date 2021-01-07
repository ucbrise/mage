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
 * @file util/registry.hpp
 * @brief Utility functions for using registries, which are global mappings
 * from name to entry initialized before main() is called.
 */

#ifndef MAGE_UTIL_REGISTRY_HPP_
#define MAGE_UTIL_REGISTRY_HPP_

#include <cassert>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <string>

namespace mage::util {
    template <typename T>
    class Register;

    /**
     * @brief Base class for registry entry types.
     *
     * Any subclass of this type should declare class Register<T>, where T is
     * the subclass, as a friend.
     */
    class BaseRegistryEntry {
    public:
        /**
         * @brief Obtain the name (label) for this registry entry.
         *
         * @return The name (label) bound to this registry entry.
         */
        const std::string& get_label() const {
            return this->label;
        }

        /**
         * @brief Obtain the human-readable description of this registry entry.
         *
         * @return The human-readable description of this registry entry.
         */
        const std::string& get_description() const {
            return this->description;
        }

    protected:
        /**
         * @brief Create a registry entry with the specified name and
         * human-readable description.
         *
         * @param name The name of this registry entry.
         * @param desc The human-readable description of this registry entry.
         */
        BaseRegistryEntry(std::string name, std::string desc) : label(name), description(desc) {
        }

    private:
        std::string label;
        std::string description;
    };

    /**
     * @brief Registry entry type storing a callable item (e.g., function
     * pointer).
     *
     * Any subclass of this type should declare class Register<T>, where T is
     * the subclass, as a friend.
     *
     * @tparam T The callable item takes a single parameter of type
     * const @p T\&.
     */
    template <typename T>
    class CallableRegistryEntry : public BaseRegistryEntry {
        friend class Register<CallableRegistryEntry>;

    public:
        /**
         * @brief Call the callable item.
         *
         * @param args Argument with which to call the callable item.
         */
        void operator ()(const T& args) const {
            this->func(args);
        }

    protected:
        /**
         * @brief Create a callable registry entry with the specified name,
         * human-readable description, and callable item.
         *
         * @param name The name of this registry entry.
         * @param desc The human-readable description of this registry entry.
         * @param f The callable item of this registry entry.
         */
        CallableRegistryEntry(const std::string& name, const std::string& desc, std::function<void(const T&)> f) : BaseRegistryEntry(name, desc), func(f) {
        }

    private:
        std::function<void(const T&)> func;
    };

    /**
     * @brief A set of static accessor functions for a global mapping from
     * names to entries that is populated at the beginning of the program,
     * before the main function is run.
     *
     * For each type of registry entry, a single global registry mapping
     * exists. This class should not be instantiated; rather, one should use
     * its static member functions to gain access to the global registry map as
     * needed.
     *
     * C++ source files may declare global variables that represent entries in
     * a registry. At runtime, the registry will contain all enries declared in
     * C++ source files compiled and linked into the build.
     *
     * Registries are used to look up programs in one of MAGE's DSLs, and
     * supported protocols in MAGE's interpreter. They make it convenient to
     * add a source file implementing a new program or new protocol and make
     * the existing command-line tools able to find the new program or
     * protocol, without any modifications to other source files.
     *
     * @tparam Entry The type of entries in this registry.
     */
    template <typename Entry>
    class Registry {
        friend class Register<Entry>;

    public:
        /**
         * @brief Registry is not constructible; it is a set of static accessor
         * member functions.
         */
        Registry() = delete;

        /**
         * @brief Returns a reference to the underlying registry mapping for
         * registry entries of type @p Entry.
         *
         * @return A const reference to the underlying registry mapping.
         */
        static const std::map<std::string, Entry>& get_registry() {
            auto& registry = Registry<Entry>::get_registry_mutable();
            return registry;
        }

        /**
         * @brief Looks up the registry entry corresponding to the specified
         * name, in the registry for registry entries of type @p Entry.
         *
         * @param name The name corresponding to the desired registry entry.
         * @return A const pointer to the registry entry if it is found, or
         * a null pointer if no registry entry corresponding to the provided
         * name is found.
         */
        static const Entry* look_up_by_name(const std::string& name) {
            auto& registry = Registry<Entry>::get_registry();
            auto iter = registry.find(name);
            if (iter == registry.end()) {
                return nullptr;
            }
            return &iter->second;
        }

        /**
         * @brief Writes the contents of the registry for registry entries of
         * type @p Entry to the specified output stream, in human-readable
         * form.
         *
         * @param plural_item The plural form of the noun describing the type
         * of item stored in the registry (e.g., "programs", "protocols").
         * @param out The output stream to which to write the human-readable
         * representation of the registry.
         */
        static void print_all(const std::string& plural_item, std::ostream& out) {
            if (Registry<Entry>::get_registry().size() == 0) {
                out << "There are no available " << plural_item << " in this build." << std::endl;
            } else {
                out << "Available " << plural_item << ":" << std::endl;
                for (const auto& [name, prot] : Registry<Entry>::get_registry()) {
                    out << name << " - " << prot.get_description() << std::endl;
                }
            }
        }

    private:
        static std::map<std::string, Entry>& get_registry_mutable() {
            static std::map<std::string, Entry> registry;
            return registry;
        }
    };

    /**
     * @brief Declaring a global variable of this type adds an entry to the
     * registry storing entries of the specified type.
     *
     * @tparam Entry The type of entry to add, specifying the registry to which
     * it is added.
     */
    template <typename Entry>
    class Register {
    public:
        /**
         * @brief Creates a new registry entry of the type @p Entry and adds
         * it to the registry containing registry entries of that type.
         *
         * @tparam Args Types of arguments to the constructor for @p Entry to
         * use to construct the registry entry, other than the description.
         * @param name The name to which the new registry entry should be bound
         * in the registry.
         * @param desc A human-readable description of the new registry entry
         * @param args Any other arguments to the @p Entry constructor, other
         * than @p desc.
         */
        template <typename... Args>
        Register(const std::string& name, const std::string& desc, Args... args) {
            std::map<std::string, Entry>& registry = Registry<Entry>::get_registry_mutable();
            auto [iter, success] = registry.emplace(name, Entry(name, desc, args...));
            if (!success) {
                std::cerr << "Trying to register \"" << name << "\" but an entry with that name already exists" << std::endl;
                std::abort();
            }
        }
    };
}

#endif
