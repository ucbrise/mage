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

    template <typename T>
    class RegistryEntry {
        friend class Register<T>;

    public:
        void operator ()(const T& args) const {
            this->func(args);
        }

        const std::string& get_description() const {
            return this->description;
        }

        static const std::map<std::string, RegistryEntry<T>>& get_registry() {
            auto& registry = RegistryEntry<T>::get_registry_mutable();
            return registry;
        }

        static const RegistryEntry<T>* look_up_by_name(const std::string& name) {
            auto& registry = RegistryEntry<T>::get_registry();
            auto iter = registry.find(name);
            if (iter == registry.end()) {
                return nullptr;
            }
            return &iter->second;
        }

    private:
        RegistryEntry<T>(const std::string& name, const std::string& desc, std::function<void(const T&)> f)
            : label(name), description(desc), func(f) {
        }

        static std::map<std::string, RegistryEntry<T>>& get_registry_mutable() {
            static std::map<std::string, RegistryEntry<T>> registry;
            return registry;
        }

        std::string label;
        std::string description;
        std::function<void(const T&)> func;
    };

    template <typename T>
    class Register {
    public:
        Register(const std::string& name, const std::string& desc, std::function<void(const T&)> f) {
            std::map<std::string, RegistryEntry<T>>& registry = RegistryEntry<T>::get_registry_mutable();
            auto [iter, success] = registry.emplace(name, RegistryEntry<T>(name, desc, f));
            if (!success) {
                std::cerr << "Trying to register \"" << name << "\" but an entry with that name already exists" << std::endl;
                std::abort();
            }
        }
    };
}

#endif
