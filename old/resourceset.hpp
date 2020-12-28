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

#ifndef MAGE_UTIL_RESOURCESET_HPP_
#define MAGE_UTIL_RESOURCESET_HPP_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>
#include "addr.hpp"

namespace mage::util {
    struct ResourceSet {
    public:
        std::string from_yaml_file(const std::string& file);

        struct Worker {
            std::optional<std::string> external_host;
            std::optional<std::string> external_port;
            std::optional<std::string> internal_host;
            std::optional<std::string> internal_port;
            std::optional<std::string> storage_path;
            std::optional<VirtPageNumber> num_available_pages;
            std::optional<VirtPageNumber> max_in_flight_swaps;
            std::optional<PageShift> page_shift;
        };

        struct Party {
            friend class ResourceSet;

        private:
            const char* init_workers(const YAML::Node& n, std::vector<Worker>& workers);

        public:
            std::optional<VirtPageNumber> default_num_available_pages;
            std::optional<VirtPageNumber> default_max_in_flight_swaps;
            std::optional<PageShift> default_page_shift;
            std::vector<Worker> workers;
        };

    private:
        std::string from_yaml(const YAML::Node& config);
        std::string init_party(const YAML::Node& party_node, Party& p);

    public:
        std::optional<VirtPageNumber> default_num_available_pages;
        std::optional<VirtPageNumber> default_max_in_flight_swaps;
        std::optional<PageShift> default_page_shift;
        std::optional<Party> garbler;
        std::optional<Party> evaluator;
    };
}

#endif
