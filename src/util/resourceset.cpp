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

#include "util/resourceset.hpp"
#include <string>
#include <yaml-cpp/yaml.h>
#include "addr.hpp"

namespace mage::util {
    static PageShift size_to_shift(PageSize size) {
        PageShift shift;
        for (shift = 0; pg_size(shift) < size; shift++) {
        }
        return shift;
    }

    std::string ResourceSet::from_yaml_file(const std::string& yaml_file) {
        YAML::Node root;
        try {
            root = YAML::LoadFile(yaml_file);
        } catch (const YAML::Exception& ye) {
            return yaml_file + ": " + ye.msg;
        }
        std::string error = this->from_yaml(root);
        if (!error.empty()) {
            return yaml_file + ": " + error;
        }
        return "";
    }

    template <typename T>
    static void init_optional(const YAML::Node& n, std::optional<T>& opt) {
        if (n) {
            opt = n.as<T>();
        } else {
            opt.reset();
        }
    }

    /*
     * If you use init_optional with T = std::uint8_t, the compiler interprets
     * it as an unsigned char and just reads the value of the character. For
     * example, 12 would be read as the character '1', not the uint8_t value
     * 12. That's we need this alternative to that function.
     */
    static void init_optional_uint8(const YAML::Node& n, std::optional<std::uint8_t>& opt) {
        if (n) {
            opt = static_cast<std::uint8_t>(n.as<std::uint16_t>());
        } else {
            opt.reset();
        }
    }


    const char* ResourceSet::Party::init_workers(const YAML::Node& n, std::vector<ResourceSet::Worker>& workers) {
        std::size_t length = n.size();
        workers.resize(length);
        for (std::size_t i = 0; i != length; i++) {
            YAML::Node worker_node = n[i];
            ResourceSet::Worker& worker = workers[i];
            init_optional(worker_node["external_host"], worker.external_host);
            init_optional(worker_node["external_port"], worker.external_port);
            init_optional(worker_node["internal_host"], worker.internal_host);
            init_optional(worker_node["internal_port"], worker.internal_port);
            init_optional(worker_node["storage_path"], worker.storage_path);
            init_optional(worker_node["num_available_pages"], worker.num_available_pages);
            init_optional(worker_node["max_in_flight_swaps"], worker.max_in_flight_swaps);
            init_optional_uint8(worker_node["page_shift"], worker.page_shift);

            std::optional<PageSize> page_size;
            init_optional(worker_node["page_size"], page_size);

            std::optional<VirtAddr> available_memory;
            init_optional(worker_node["available_memory"], available_memory);

            if (worker.page_shift.has_value() && page_size.has_value()) {
                return "page_shift and page_size should not both be set";
            } else if (page_size.has_value()) {
                worker.page_shift = size_to_shift(page_size.value());
            }

            if (worker.num_available_pages.has_value() && available_memory.has_value()) {
                return "num_available_pages and available_memory should not both be set";
            } else if (available_memory.has_value()) {
                if (worker.page_shift.has_value()) {
                    PageShift shift = worker.page_shift.value();
                    worker.num_available_pages = pg_num(available_memory.value(), shift);
                } else {
                    return "cannot use num_available_pages unless page size is known";
                }
            }

            if (!worker.num_available_pages.has_value() && this->default_num_available_pages.has_value()) {
                worker.num_available_pages = this->default_num_available_pages.value();
            }
            if (!worker.max_in_flight_swaps.has_value() && this->default_max_in_flight_swaps.has_value()) {
                worker.max_in_flight_swaps = this->default_max_in_flight_swaps.value();
            }
            if (!worker.page_shift.has_value() && this->default_page_shift.has_value()) {
                worker.page_shift = this->default_page_shift.value();
            }

            if (worker.max_in_flight_swaps.has_value() && worker.max_in_flight_swaps.value() >= worker.num_available_pages.value()) {
                return "must have at least one page available for each in-flight swap";
            }
        }
        return nullptr;
    }

    std::string ResourceSet::init_party(const YAML::Node& party_node, ResourceSet::Party& p) {
        if (!party_node.IsMap()) {
            return "expected map at party level";
        }
        try {
            init_optional(party_node["default_num_available_pages"], p.default_num_available_pages);
            init_optional(party_node["default_max_in_flight_swaps"], p.default_max_in_flight_swaps);
            init_optional_uint8(party_node["default_page_shift"], p.default_page_shift);

            if (!p.default_num_available_pages.has_value() && this->default_num_available_pages.has_value()) {
                p.default_num_available_pages = this->default_num_available_pages.value();
            }
            if (!p.default_max_in_flight_swaps.has_value() && this->default_max_in_flight_swaps.has_value()) {
                p.default_max_in_flight_swaps = this->default_max_in_flight_swaps.value();
            }
            if (!p.default_page_shift.has_value() && this->default_page_shift.has_value()) {
                p.default_page_shift = this->default_page_shift.value();
            }
            if (party_node["workers"]) {
                const char* err = p.init_workers(party_node["workers"], p.workers);
                if (err != nullptr) {
                    return "workers: " + std::string(err);
                }
            } else {
                p.workers.resize(0);
            }
        } catch (const YAML::Exception& ye) {
            return ye.msg;
        }
        return "";
    }

    std::string ResourceSet::from_yaml(const YAML::Node& config) {
        if (!config.IsMap()) {
            return "expected map at top level";
        }
        init_optional(config["default_num_available_pages"], this->default_num_available_pages);
        init_optional(config["default_max_in_flight_swaps"], this->default_max_in_flight_swaps);
        init_optional_uint8(config["default_page_shift"], this->default_page_shift);
        if (config["garbler"]) {
            this->garbler.emplace();
            std::string err = this->init_party(config["garbler"], this->garbler.value());
            if (!err.empty()) {
                return err;
            }
        } else {
            this->garbler.reset();
        }
        if (config["evaluator"]) {
            this->evaluator.emplace();
            std::string err = this->init_party(config["evaluator"], this->evaluator.value());
            if (!err.empty()) {
                return err;
            }
        } else {
            this->evaluator.reset();
        }
        return "";
    }
}
