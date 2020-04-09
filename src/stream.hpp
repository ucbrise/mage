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

#ifndef MAGE_STREAM_HPP_
#define MAGE_STREAM_HPP_

#include <fstream>
#include <iostream>
#include <vector>

namespace mage {
    template <typename T>
    class StreamReader {
    public:
        virtual ~StreamReader() {
        }
        virtual bool next(T& item) = 0;
        virtual std::uint64_t length() = 0;
    };

    template <typename T>
    class FileStreamReader : public StreamReader<T> {
    public:
        FileStreamReader(std::string filename) :total_read(0) {
            this->input.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
            this->input.open(filename, std::ios::in | std::ios::binary);
            this->input.seekg(0, std::ios::end);
            std::streampos length = this->input.tellg();
            this->size = length / sizeof(T);
            this->input.seekg(0, std::ios::beg);
        }

        bool next(T& item) override {
            if (this->total_read == this->size) {
                return false;
            }
            this->input.read(reinterpret_cast<char*>(&item), sizeof(item));
            this->total_read++;
            return true;
        }

        std::uint64_t length() override {
            return this->size;
        }

    protected:
        std::uint64_t size;
        std::uint64_t total_read;
        std::ifstream input;
    };

    template <typename T>
    class StreamWriter {
    public:
        virtual ~StreamWriter() {
        }
        virtual void append(const T& item) = 0;
    };

    template <typename T>
    class FileStreamWriter : public StreamWriter<T> {
    public:
        FileStreamWriter(std::string filename) {
            this->output.exceptions(std::ios::failbit | std::ios::badbit);
            this->output.open(filename, std::ios::out | std::ios::binary | std::ios::trunc);
        }

        void append(const T& item) override {
            this->output.write(reinterpret_cast<const char*>(&item), sizeof(item));
        }

    protected:
        std::ofstream output;
    };

    template <typename T>
    class VectorStreamWriter : public StreamWriter<T> {
    public:
        VectorStreamWriter(std::vector<T>* store_into) : stream(store_into) {
        }

        void append(const T& item) override {
            this->stream->push_back(item);
        }

    private:
        std::vector<T>* stream;
    };
}

#endif
