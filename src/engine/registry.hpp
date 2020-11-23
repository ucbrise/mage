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

#ifndef MAGE_PROGRAMS_REGISTRY_HPP_
#define MAGE_PROGRAMS_REGISTRY_HPP_

#include <memory>
#include "engine/cluster.hpp"
#include "util/config.hpp"
#include "util/registry.hpp"

namespace mage::engine {
    struct EngineOptions {
        util::Configuration* config;
        std::uint32_t party_id;
        WorkerID self_id;
        std::shared_ptr<ClusterNetwork> cluster;
        std::string problem_name;
    };

    using RegisterProtocol = util::Register<EngineOptions>;
    using RegisteredProtocol = util::RegistryEntry<EngineOptions>;
}

#endif
