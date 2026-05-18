# Tests (GoogleTest)
find_package(GTest REQUIRED)

include(GoogleTest)

add_executable(DeexcitationTest TestCOLA.cpp)

set_target_properties(
    DeexcitationTest
    PROPERTIES CXX_STANDARD 20
               CXX_STANDARD_REQUIRED YES
               CXX_EXTENSIONS NO
               CXX_CLANG_TIDY "${COLA_CMAKE_CXX_CLANG_TIDY}"
)

target_link_libraries(DeexcitationTest ${COLA_MODULE})
target_link_libraries(DeexcitationTest GTest::GTest GTest::Main)

gtest_discover_tests(DeexcitationTest)
