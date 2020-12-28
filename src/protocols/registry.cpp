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

#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <optional>
#include <string>
#include <vector>
#include "protocols/registry.hpp"

namespace mage::protocols {
    std::vector<std::string> evaluator_synonyms = { "evaluator", "0", "bob" };
    std::vector<std::string> garbler_synonyms = { "garbler", "1", "alice" };

    std::optional<PartyID> parse_party_id(const std::string& party) {
        if (std::find(evaluator_synonyms.begin(), evaluator_synonyms.end(), party) != evaluator_synonyms.end()) {
            return evaluator_party_id;
        } else if (std::find(garbler_synonyms.begin(), garbler_synonyms.end(), party) != garbler_synonyms.end()) {
            return garbler_party_id;
        } else {
            std::size_t length;
            unsigned long long party_id = std::stoull(party, &length);
            if (length != party.length()) {
                return {};
            }
            return static_cast<PartyID>(party_id);
        }
    }

    memprog::AllocationSize identity_physical_size(std::uint64_t logical_width, memprog::PlaceableType type) {
        return logical_width;
    }

    RegisterPlacementPlugin identity_plugin("identity_plugin", "Object's MAGE-virtual size is its logical width", identity_physical_size);
}
