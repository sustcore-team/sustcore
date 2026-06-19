/**
 * @file tree.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief FDT 树表示
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/fdt/types.h>
#include <sustcore/addr.h>
#include <sustcore/boot.h>
#include <sustcore/errcode.h>

#include <cassert>
#include <string>
#include <unordered_map>
#include <vector>

namespace fdt {
    namespace detail {
        [[nodiscard]]
        constexpr bool parse_be_integer(const char *data, size_t size,
                                        uint64_t &value) {
            if (data == nullptr || size == 0 || size > sizeof(uint64_t)) {
                return false;
            }

            value = 0;
            for (size_t i = 0; i < size; ++i) {
                value = (value << 8) | static_cast<unsigned char>(data[i]);
            }
            return true;
        }
    }  // namespace detail

    struct Property {
        std::string name;
        const char *data;
        size_t size;

        [[nodiscard]]
        constexpr std::vector<std::string> as_string_list() const {
            std::vector<std::string> result;
            std::string str = "";
            for (int i = 0; i < size; i++) {
                if (data[i] == '\0') {
                    if (!str.empty()) {
                        result.push_back(str);
                        str.clear();
                    }
                } else {
                    str += data[i];
                }
            }
            if (!str.empty()) {
                result.push_back(str);
            }
            return result;
        }

        [[nodiscard]]
        constexpr std::vector<PhyArea> as_regions(
            const RegionCells &cells) const {
            constexpr size_t CELL_SIZE = sizeof(sus_u32);

            if (data == nullptr || cells.addr_cells == 0 ||
                cells.size_cells == 0)
            {
                assert(false && "invalid property buffer or region cells");
                return {};
            }

            size_t total_cells      = cells.addr_cells + cells.size_cells;
            size_t bytes_per_region = total_cells * CELL_SIZE;
            size_t addr_size        = cells.addr_cells * CELL_SIZE;
            size_t size_size        = cells.size_cells * CELL_SIZE;

            if (bytes_per_region == 0 || size % bytes_per_region != 0) {
                assert(false && "invalid property size for reg regions");
                return {};
            }
            if (addr_size > sizeof(uint64_t) || size_size > sizeof(uint64_t)) {
                assert(false && "reg cell width exceeds uint64_t");
                return {};
            }

            std::vector<PhyArea> result;
            result.reserve(size / bytes_per_region);
            for (size_t offset = 0; offset < size; offset += bytes_per_region) {
                uint64_t begin_raw = 0;
                uint64_t size_raw  = 0;
                bool ok = detail::parse_be_integer(data + offset, addr_size,
                                                   begin_raw) &&
                          detail::parse_be_integer(data + offset + addr_size,
                                                   size_size, size_raw);
                if (!ok ||
                    size_raw > static_cast<uint64_t>(MAX_ADDR) - begin_raw)
                {
                    assert(false && "invalid reg region value");
                    return {};
                }

                result.push_back(PhyArea(
                    PhyAddr(static_cast<addr_t>(begin_raw)),
                    PhyAddr(static_cast<addr_t>(begin_raw + size_raw))));
            }
            return result;
        }

        [[nodiscard]]
        constexpr uint64_t as_integral() const {
            uint64_t value = 0;
            if (!detail::parse_be_integer(data, size, value)) {
                assert(false && "invalid property integral width");
                return 0;
            }
            return value;
        }
        [[nodiscard]]
        constexpr phandle_t as_phandle() const {
            return as_integral();
        }
        [[nodiscard]]
        constexpr std::string as_string() const {
            return as_string_list().empty() ? "" : as_string_list()[0];
        }
        [[nodiscard]]
        constexpr std::vector<byte> as_byte_array() const {
            return {data, data + size};
        }
    };

    struct Node {
        std::string name;
        std::unordered_map<std::string, util::owner<Property *>> properties;
        std::unordered_map<std::string, util::owner<Node *>> children;
        Node *parent      = nullptr;
        phandle_t phandle = 0;

        Node() = default;

        Node(const Node &other);
        Node &operator=(const Node &other);
        Node(Node &&other);
        Node &operator=(Node &&other);

        void cleanup();

        ~Node();
    };

    struct Configuration {
        util::owner<Node *> root;
        std::unordered_map<phandle_t, Node *> phandle_map;

        [[nodiscard]]
        Node *get_node_by_path(const std::string &path) const;
        [[nodiscard]]
        Node *get_node_by_phandle(phandle_t phandle) const;
        [[nodiscard]]
        Result<void> add_node(Node *parent, util::owner<Node *> new_node);
        [[nodiscard]]
        Result<void> remove_node(Node *node);
    };

    void make_config(void *dtb, Configuration &config);
}  // namespace fdt
