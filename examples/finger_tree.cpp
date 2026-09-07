// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/fingertree/fingertree.hpp>

#include <cstddef>
#include <iostream>
#include <string>

int main() {
    namespace ft = beman::fingertree::tree;

    // 78d295c0-0601-4b09-b262-7c18620d3034
    // Build a persistent measured sequence 0..9; snoc appends at the back.
    ft::FingerTree<int> seq;
    for (int i = 0; i < 10; ++i)
        seq = seq.snoc(i);

    std::cout << "sequence size (measure): " << seq.measure() << '\n';

    // Split by accumulated measure: the left side is the longest prefix whose
    // measure stays below the threshold; the element that reaches it goes right.
    auto split = seq.split_at_measure(std::size_t{4});
    std::cout << "left size:  " << split.d_left.measure() << '\n';
    std::cout << "right size: " << split.d_right.measure() << '\n';

    // Persistence: the split shares structure and leaves the original intact.
    std::cout << "original still has " << seq.measure() << " elements\n";
    // 78d295c0-0601-4b09-b262-7c18620d3034 end

    // A rope is a capability wrapper over the same tree, measured in bytes.
    auto rope   = ft::FingerTreeRope<>::from_text("hello world", 4);
    auto edited = rope.insert(5, ",");
    std::cout << "rope: \"" << edited.to_string() << "\" (" << edited.size_bytes() << " bytes)\n";

    return 0;
}
