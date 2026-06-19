/**
 * @file tree.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief FDT 树表示实现
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/description.h>
#if defined(__ARCH_riscv64__)
#include <arch/riscv64/device/fdt_helper.h>
#elif defined(__ARCH_loongarch64__)
#include <arch/loongarch64/device/fdt_helper.h>
#endif
#include <device/fdt/decode.h>
#include <device/fdt/tree.h>

namespace {
    /**
     * @brief 递归构建设备树节点对象.
     */
    void build_node_recursive(const void *dtb, fdt::Configuration &config,
                              fdt::Node &node, int node_offset) {
        int prop_offset;
        fdt_for_each_property_offset(prop_offset, dtb, node_offset) {
            const char *prop_name = nullptr;
            int prop_len          = 0;
            const void *prop_data =
                fdt_getprop_by_offset(dtb, prop_offset, &prop_name, &prop_len);
            if (prop_name == nullptr || prop_data == nullptr || prop_len < 0) {
                continue;
            }

            auto *prop = new fdt::Property{
                .name = prop_name,
                .data = static_cast<const char *>(prop_data),
                .size = static_cast<size_t>(prop_len),
            };
            node.properties[prop->name] = util::owner(prop);

            if ((prop->name == fdt::PHANDLE_PROP ||
                 prop->name == fdt::LINUX_PHANDLE_PROP) &&
                node.phandle == 0)
            {
                node.phandle = prop->as_phandle();
            }
        }

        if (node.phandle != 0) {
            config.phandle_map[node.phandle] = &node;
        }

        int child_offset;
        fdt_for_each_subnode(child_offset, dtb, node_offset) {
            const char *child_name = fdt_get_name(dtb, child_offset, nullptr);
            if (child_name == nullptr) {
                continue;
            }

            auto child    = util::owner(new fdt::Node());
            child->name   = child_name;
            child->parent = &node;

            build_node_recursive(dtb, config, *child, child_offset);
            node.children[child->name] = std::move(child);
        }
    }
}  // namespace

namespace fdt {
    Node::Node(const Node &other)
        : name(other.name),
          properties(),
          children(),
          parent(other.parent),
          phandle(other.phandle) {
        for (const auto &[key, prop] : other.properties) {
            properties[key] = util::owner(new Property(*prop));
        }
        for (const auto &[key, child] : other.children) {
            children[key] = util::owner(new Node(*child));
        }
    }

    Node &Node::operator=(const Node &other) {
        if (this != &other) {
            cleanup();

            name    = other.name;
            parent  = other.parent;
            phandle = other.phandle;
            for (const auto &[key, prop] : other.properties) {
                properties[key] = util::owner(new Property(*prop));
            }
            for (const auto &[key, child] : other.children) {
                children[key] = util::owner(new Node(*child));
            }
        }
        return *this;
    }

    Node::Node(Node &&other)
        : name(std::move(other.name)),
          properties(std::move(other.properties)),
          children(std::move(other.children)),
          parent(other.parent),
          phandle(other.phandle) {}

    Node &Node::operator=(Node &&other) {
        if (this != &other) {
            cleanup();
            name       = std::move(other.name);
            properties = std::move(other.properties);
            children   = std::move(other.children);
            parent     = other.parent;
            phandle    = other.phandle;
        }
        return *this;
    }

    void Node::cleanup() {
        for (auto &[_, prop] : properties) {
            delete prop;
        }
        for (auto &[_, child] : children) {
            delete child;
        }
        properties.clear();
        children.clear();
    }

    Node::~Node() {
        cleanup();
    }

    Node *Configuration::get_node_by_path(const std::string &path) const {
        if (!root || root.get() == nullptr) {
            return nullptr;
        }
        if (path.empty() || path == "/") {
            return root.get();
        }

        Node *current    = root.get();
        size_t component = 0;
        while (component < path.size()) {
            while (component < path.size() && path[component] == '/') {
                ++component;
            }
            if (component >= path.size()) {
                break;
            }

            size_t end = component;
            while (end < path.size() && path[end] != '/') {
                ++end;
            }

            std::string name = path.substr(component, end - component);
            auto child_it    = current->children.find(name);
            if (child_it == current->children.end()) {
                return nullptr;
            }
            current   = child_it->second.get();
            component = end;
        }
        return current;
    }

    Node *Configuration::get_node_by_phandle(phandle_t phandle) const {
        auto it = phandle_map.find(phandle);
        return it == phandle_map.end() ? nullptr : it->second;
    }

    Result<void> Configuration::add_node(Node *parent,
                                         util::owner<Node *> new_node) {
        if (new_node.get() == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        if (!root) {
            if (parent != nullptr) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            root         = std::move(new_node);
            root->parent = nullptr;
            if (root->phandle != 0) {
                phandle_map[root->phandle] = root.get();
            }
            void_return();
        }

        if (parent == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        new_node->parent = parent;
        if (parent->children.contains(new_node->name)) {
            unexpect_return(ErrCode::BUSY);
        }

        if (new_node->phandle != 0) {
            phandle_map[new_node->phandle] = new_node.get();
        }
        parent->children[new_node->name] = std::move(new_node);
        void_return();
    }

    Result<void> Configuration::remove_node(Node *node) {
        if (node == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (!root || root.get() == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        if (node == root.get()) {
            if (root->phandle != 0) {
                phandle_map.erase(root->phandle);
            }
            root = util::owner<Node *>(nullptr);
            void_return();
        }

        Node *parent = node->parent;
        if (parent == nullptr) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        auto it = parent->children.find(node->name);
        if (it == parent->children.end() || it->second.get() != node) {
            unexpect_return(ErrCode::ENTRY_NOT_FOUND);
        }

        if (node->phandle != 0) {
            phandle_map.erase(node->phandle);
        }
        parent->children.erase(it);
        void_return();
    }

    void make_config(void *dtb, Configuration &config) {
        config.root = util::owner<Node *>(nullptr);
        config.phandle_map.clear();

        if (dtb == nullptr) {
            assert(false && "dtb must not be null");
            return;
        }
        if (FDTHelper::fdt_init(dtb) == nullptr) {
            assert(false && "fdt_init failed");
            return;
        }

        auto root    = util::owner(new Node());
        root->name   = "/";
        root->parent = nullptr;
        build_node_recursive(static_cast<const void *>(FDTHelper::fdt), config,
                             *root, FDTHelper::get_root_node());
        config.root = std::move(root);
    }
}  // namespace fdt
