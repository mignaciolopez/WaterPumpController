#pragma once

#include <gtest/gtest.h>

// Include once, at the end of each test suite's source file.
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
