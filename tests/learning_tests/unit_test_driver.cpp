#include <gtest/gtest.h>
#include "gtest_test.h"
#include "opencl_vector_test.h"

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
