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

#ifndef MAGE_PROTOCOLS_REGISTRY_HPP_
#define MAGE_PROTOCOLS_REGISTRY_HPP_

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include "addr.hpp"
#include "engine/cluster.hpp"
#include "memprog/placement.hpp"
#include "util/config.hpp"
#include "util/registry.hpp"

namespace mage::protocols {
    struct EngineOptions {
        util::Configuration* config;
        PartyID party_id;
        WorkerID self_id;
        std::shared_ptr<engine::ClusterNetwork> cluster;
        std::string problem_name;
    };

    extern std::vector<std::string> evaluator_synonyms;
    extern std::vector<std::string> garbler_synonyms;

    std::optional<PartyID> parse_party_id(const std::string& party);

    class RegisteredPlacementPlugin : public util::BaseRegistryEntry {
        friend class util::Register<RegisteredPlacementPlugin>;

    public:
        memprog::PlacementPlugin get_placement_plugin() const {
            return this->p;
        }

    private:
        RegisteredPlacementPlugin(std::string name, std::string desc, memprog::PlacementPlugin plugin)
            : util::BaseRegistryEntry(name, desc), p(plugin) {
        }

        memprog::PlacementPlugin p;
    };

    using RegisterPlacementPlugin = util::Register<RegisteredPlacementPlugin>;

    class RegisteredProtocol : public util::CallableRegistryEntry<EngineOptions> {
        friend class util::Register<RegisteredProtocol>;

    public:
        const std::string& get_placement_plugin_name() const {
            return this->plugin_name;
        }

        memprog::PlacementPlugin get_placement_plugin() const {
            const std::string& name = this->get_placement_plugin_name();
            const RegisteredPlacementPlugin* plugin_ptr = util::Registry<RegisteredPlacementPlugin>::look_up_by_name(name);
            if (plugin_ptr == nullptr) {
                std::cerr << "Misconfigured build: protocol \"" << this->get_label() << "\" requires placement plugin \"" << name << "\"" << std::endl;
                std::abort();
            }
            return plugin_ptr->get_placement_plugin();
        }

    private:
        RegisteredProtocol(std::string name, std::string desc, std::function<void(const EngineOptions&)> driver, const std::string& plugin)
            : util::CallableRegistryEntry<EngineOptions>(name, desc, driver), plugin_name(plugin) {
        }

        std::string plugin_name;
    };

    using RegisterProtocol = util::Register<RegisteredProtocol>;
}

#endif
