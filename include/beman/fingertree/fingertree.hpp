// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef BEMAN_FINGERTREE_FINGERTREE_HPP
#define BEMAN_FINGERTREE_FINGERTREE_HPP

#include <beman/fingertree/config.hpp>

#if BEMAN_FINGERTREE_USE_MODULES() && !defined(BEMAN_FINGERTREE_INCLUDED_FROM_INTERFACE_UNIT)

import beman.fingertree;

#else

    #include <beman/fingertree/todo.hpp>

#endif // BEMAN_FINGERTREE_USE_MODULES() &&
       // !defined(BEMAN_FINGERTREE_INCLUDED_FROM_INTERFACE_UNIT)

#endif // BEMAN_FINGERTREE_FINGERTREE_HPP
