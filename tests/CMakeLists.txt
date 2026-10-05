# Tests unitaires du cœur métier (Qt Test). Voir Phase 2 du plan.
add_executable(tst_core tst_core.cpp)
target_link_libraries(tst_core PRIVATE grossbuch_core Qt6::Test)

target_compile_options(tst_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

add_test(NAME tst_core COMMAND tst_core)
