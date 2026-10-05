cmake_minimum_required(VERSION 3.21)

project(grossbuch
    VERSION 0.1.0
    DESCRIPTION "Comptabilité privée"
    LANGUAGES CXX
)

# --- Standard C++ -----------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Exporte compile_commands.json pour clang-tidy et les IDE.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Intégration Qt ---------------------------------------------------------
# Génération automatique du code moc / uic / rcc.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Type de build par défaut (si l'utilisateur n'en fournit pas).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Type de build" FORCE)
endif()

# Le composant Charts sera ajouté dans sa phase dédiée.
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Test)

enable_testing()

# --- Bibliothèque de logique métier (sans dépendance à Qt Widgets) ----------
# Voir docs/adr/0004.
add_library(grossbuch_core STATIC
    src/core/Database.cpp
    src/core/CategoryRepository.cpp
    src/core/ExpenseRepository.cpp
)

target_include_directories(grossbuch_core PUBLIC src)
target_link_libraries(grossbuch_core PUBLIC Qt6::Core Qt6::Sql)

target_compile_options(grossbuch_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

# --- Cible applicative ------------------------------------------------------
add_executable(grossbuch
    src/main.cpp
    src/ui/MainWindow.cpp
)

target_link_libraries(grossbuch PRIVATE Qt6::Widgets grossbuch_core)

target_compile_options(grossbuch PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

# --- Installation -----------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS grossbuch RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# --- Tests ------------------------------------------------------------------
add_subdirectory(tests)
