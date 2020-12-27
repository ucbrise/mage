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

    class BaseRegistryEntry {
    public:
        const std::string& get_label() {
            return this->label;
        }

        const std::string& get_description() const {
            return this->description;
        }

    protected:
        BaseRegistryEntry(std::string name, std::string desc) : label(name), description(desc) {
        }

    private:
        std::string label;
        std::string description;
    };

    template <typename T>
    class CallableRegistryEntry : public BaseRegistryEntry {
        friend class Register<CallableRegistryEntry>;

    public:
        void operator ()(const T& args) const {
            this->func(args);
        }

    protected:
        CallableRegistryEntry(const std::string& name, const std::string& desc, std::function<void(const T&)> f) : BaseRegistryEntry(name, desc), func(f) {
        }

    private:
        std::function<void(const T&)> func;
    };

    // template <typename T>
    // class RegistryEntry {
    //     friend class Register<T>;
    //
    // public:
    //     void operator ()(const T& args) const {
    //         this->func(args);
    //     }
    //
    //     const std::string& get_description() const {
    //         return this->description;
    //     }
    //
    // private:
    //     RegistryEntry<T>(const std::string& name, const std::string& desc, std::function<void(const T&)> f)
    //         : label(name), description(desc), func(f) {
    //     }
    //
    //     std::string label;
    //     std::string description;
    //     std::function<void(const T&)> func;
    // };

    template <typename Entry>
    class Registry {
        friend class Register<Entry>;

    public:
        static const std::map<std::string, Entry>& get_registry() {
            auto& registry = Registry<Entry>::get_registry_mutable();
            return registry;
        }

        static const Entry* look_up_by_name(const std::string& name) {
            auto& registry = Registry<Entry>::get_registry();
            auto iter = registry.find(name);
            if (iter == registry.end()) {
                return nullptr;
            }
            return &iter->second;
        }

    private:
        static std::map<std::string, Entry>& get_registry_mutable() {
            static std::map<std::string, Entry> registry;
            return registry;
        }
    };

    template <typename Entry>
    class Register {
    public:
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
