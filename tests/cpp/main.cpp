// main.cpp — entry point for the C++ unit-test binary.
//
// Every test_*.cpp translation unit self-registers its cases via the
// TEST_CASE macro; this file just runs them.

#include "test_framework.hpp"

int main(int argc, char **argv)
{
    return ::mt::run_all(argc, argv);
}
