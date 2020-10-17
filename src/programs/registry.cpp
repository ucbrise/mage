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

#include <cassert>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include "memprog/program.hpp"
#include "programs/registry.hpp"

namespace mage::programs {
    RegisterProgram::RegisterProgram(const std::string& program_name, const std::string& program_desc, std::function<void(const ProgramOptions&)> program_function) {
        auto& registry = RegisteredProgram::get_registry_mutable();
        auto [iter, success] = registry.emplace(program_name, RegisteredProgram(program_name, program_desc, program_function));
        if (!success) {
            std::cerr << "Trying to register program with name \"" << program_name << "\" but a program with that name already exists" << std::endl;
            std::abort();
        }
    }

    memprog::DefaultProgram* RegisteredProgram::program_ptr = nullptr;

    RegisteredProgram::RegisteredProgram(const std::string& program_name, const std::string& program_desc, std::function<void(const ProgramOptions&)> program_function)
        : name(program_name), description(program_desc), program(program_function) {
    }

    void RegisteredProgram::operator ()(const ProgramOptions& args) const {
        this->program(args);
    }

    const std::string& RegisteredProgram::get_description() const {
        return this->description;
    }

    const std::map<std::string, RegisteredProgram>& RegisteredProgram::get_registry() {
        auto& registry = RegisteredProgram::get_registry_mutable();
        return registry;
    }

    std::map<std::string, RegisteredProgram>& RegisteredProgram::get_registry_mutable() {
        static std::map<std::string, RegisteredProgram> registry;
        return registry;
    }

    const RegisteredProgram* RegisteredProgram::look_up_by_name(const std::string& name) {
        auto& registry = RegisteredProgram::get_registry();
        auto iter = registry.find(name);
        if (iter == registry.end()) {
            return nullptr;
        }
        return &iter->second;
    }
}
