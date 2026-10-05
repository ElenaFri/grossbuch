# Tests unitaires du cœur métier (Qt Test). Voir Phase 2 du plan.
add_executable(tst_core tst_core.cpp)
target_link_libraries(tst_core PRIVATE grossbuch_core Qt6::Test)

target_compile_options(tst_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

add_test(NAME tst_core COMMAND tst_core)

# Tests de l'onglet de saisie (Qt Test, Widgets). Voir Phase 4 du plan.
add_executable(tst_entrytab tst_entrytab.cpp)
target_link_libraries(tst_entrytab PRIVATE grossbuch_ui Qt6::Test)

target_compile_options(tst_entrytab PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

add_test(NAME tst_entrytab COMMAND tst_entrytab)
# Exécution sans serveur graphique.
set_tests_properties(tst_entrytab PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Tests de l'onglet Récapitulatif (Qt Test, Widgets). Voir Phase 5 du plan.
add_executable(tst_summarytab tst_summarytab.cpp)
target_link_libraries(tst_summarytab PRIVATE grossbuch_ui Qt6::Test)

target_compile_options(tst_summarytab PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

add_test(NAME tst_summarytab COMMAND tst_summarytab)
# Exécution sans serveur graphique.
set_tests_properties(tst_summarytab PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Tests de l'onglet Graphiques (Qt Test, Widgets, Charts). Voir Phase 6 du plan.
add_executable(tst_chartstab tst_chartstab.cpp)
target_link_libraries(tst_chartstab PRIVATE grossbuch_ui Qt6::Test)

target_compile_options(tst_chartstab PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

add_test(NAME tst_chartstab COMMAND tst_chartstab)
# Exécution sans serveur graphique.
set_tests_properties(tst_chartstab PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
