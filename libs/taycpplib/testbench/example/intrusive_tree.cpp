/**
 * @file intrusive_tree.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 演示 Tay 非拥有式侵入层级树。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/intrusive.h>
#include <tay/tree.h>

#include <cstdio>

namespace {
    struct node {
        const char* name;
        tay::intrusive_tree_hook<node> hook;
    };

    using locate_node = tay::locate_member<node, tay::intrusive_tree_hook<node>, &node::hook>;
    using node_tree   = tay::intrusive_tree<node, locate_node>;
}  // namespace

int main() {
    node root{"root"};
    node devices{"devices"};
    node memory{"memory"};
    node uart{"uart"};

    node_tree tree;
    tree.link_back(root, devices);
    tree.link_back(root, memory);
    tree.link_back(devices, uart);

    std::printf("preorder:");
    for (node* current : tree.preorder(root)) {
        std::printf(" %s", current->name);
    }
    std::printf("\n");

    std::printf("postorder:");
    for (node* current : tree.postorder(root)) {
        std::printf(" %s", current->name);
    }
    std::printf("\n");

    std::printf("uart depth=%zu, lca(uart, memory)=%s\n", tree.depth(uart),
                tree.lca(&uart, &memory)->name);
    tree.reparent(memory, uart);
    std::printf("uart new parent=%s\n", tree.parent(uart)->name);
    return 0;
}
