# Tests unitaires du cœur métier (Qt Test). Voir Phase 2 du plan.
add_executable(tst_core tst_core.cpp)
target_link_libraries(tst_core PRIVATE grossbuch_core Qt6::Test)

target_compile_options(tst_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

add_test(NAME tst_core COMMAND tst_core)

# Tests unitaires des paiements récurrents (Qt Test). Voir Phase 8 du plan.
add_executable(tst_recurring tst_recurring.cpp)
target_link_libraries(tst_recurring PRIVATE grossbuch_core Qt6::Test)

target_compile_options(tst_recurring PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

add_test(NAME tst_recurring COMMAND tst_recurring)

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

# Tests de la persistance de l'état de l'UI (Qt Test, Widgets). Voir Phase 7.
add_executable(tst_uistate tst_uistate.cpp)
target_link_libraries(tst_uistate PRIVATE grossbuch_ui Qt6::Test)

target_compile_options(tst_uistate PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

add_test(NAME tst_uistate COMMAND tst_uistate)
# Exécution sans serveur graphique.
set_tests_properties(tst_uistate PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Tests de l'onglet Paiements récurrents (Qt Test, Widgets). Voir Phase 8 du plan.
add_executable(tst_recurringtab tst_recurringtab.cpp)
target_link_libraries(tst_recurringtab PRIVATE grossbuch_ui Qt6::Test)

target_compile_options(tst_recurringtab PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

add_test(NAME tst_recurringtab COMMAND tst_recurringtab)
# Exécution sans serveur graphique.
set_tests_properties(tst_recurringtab PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
