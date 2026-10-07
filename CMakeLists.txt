cmake_minimum_required(VERSION 3.21)

project(grossbuch
    VERSION 0.2.0
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

# Le composant Charts est requis pour l'onglet des graphiques annuels (phase 6).
find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Test Charts)

enable_testing()

# --- Bibliothèque de logique métier (sans dépendance à Qt Widgets) ----------
# Voir docs/adr/0004.
add_library(grossbuch_core STATIC
    src/core/Database.cpp
    src/core/CategoryRepository.cpp
    src/core/ExpenseRepository.cpp
    src/core/RecurringRepository.cpp
    src/core/ExchangeService.cpp
    src/core/BackupService.cpp
    src/core/DataController.cpp
)

target_include_directories(grossbuch_core PUBLIC src)
target_link_libraries(grossbuch_core PUBLIC Qt6::Core Qt6::Sql)

target_compile_options(grossbuch_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

# --- Bibliothèque d'interface (Qt Widgets) ----------------------------------
add_library(grossbuch_ui STATIC
    src/ui/MainWindow.cpp
    src/ui/EntryTab.cpp
    src/ui/RecurringTab.cpp
    src/ui/SummaryTab.cpp
    src/ui/ChartsTab.cpp
    src/ui/DataTab.cpp
    src/ui/UiHelpers.cpp
)

target_include_directories(grossbuch_ui PUBLIC src)
target_link_libraries(grossbuch_ui PUBLIC Qt6::Widgets Qt6::Charts grossbuch_core)

target_compile_options(grossbuch_ui PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

# --- Cible applicative ------------------------------------------------------
add_executable(grossbuch
    src/main.cpp
)

target_link_libraries(grossbuch PRIVATE grossbuch_ui)

target_compile_options(grossbuch PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>
)

# --- Installation -----------------------------------------------------------
include(GNUInstallDirs)

# Force les permissions 0755 sur les répertoires créés à l'installation (sinon
# l'umask 002 produit du 0775, non conforme aux règles Debian).
set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

# Binaire.
install(TARGETS grossbuch RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Intégration au bureau : lanceur et icône scalable (thème hicolor).
install(FILES packaging/grossbuch.desktop
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)
install(FILES packaging/grossbuch.svg
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
)

# Documentation du paquet : copyright (DEP-5) et journal des modifications
# compressé, attendus par les règles Debian sous usr/share/doc/grossbuch/.
install(FILES packaging/copyright
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

# Compresse le changelog à la configuration (gzip -9 -n pour la reproductibilité).
find_program(GZIP_TOOL gzip)
if(GZIP_TOOL)
    execute_process(
        COMMAND ${GZIP_TOOL} -9 -n -c ${CMAKE_SOURCE_DIR}/packaging/changelog.Debian
        OUTPUT_FILE ${CMAKE_BINARY_DIR}/changelog.Debian.gz
    )
    install(FILES ${CMAKE_BINARY_DIR}/changelog.Debian.gz
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
    )
endif()

# --- Paquet .deb (CPack) ----------------------------------------------------
# Voir docs/adr/0005.
set(CPACK_PACKAGE_NAME "grossbuch")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_DESCRIPTION
    "grossbuch est une application de bureau de comptabilité personnelle.\n\
Elle permet de saisir les dépenses par catégories et sous-catégories, de\n\
définir des paiements récurrents reportés automatiquement, de consulter un\n\
récapitulatif mensuel ou annuel et des graphiques annuels\n\
superposés. Les données sont conservées dans une base SQLite locale.")
set(CPACK_PACKAGE_CONTACT "Elena FRISON <e_gavrilova_frison@zohomail.eu>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/ElenaFri/grossbuch")

# Retire les symboles de débogage du binaire installé.
set(CPACK_STRIP_FILES ON)

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
# Dépendances déclarées à la main pour un paquet portable entre distributions.
# dpkg-shlibdeps figerait les noms et versions des paquets Qt de la machine de
# build ; or la transition « 64-bit time_t » a renommé certains paquets avec le
# suffixe t64 de façon divergente (Debian 13 : seul libqt6core6 ; Ubuntu : core,
# gui, sql et widgets). On le désactive et on liste des alternatives « t64 | sans
# t64 » avec un plancher de version prudent, pour que le même .deb s'installe sur
# Debian 12/13, LMDE, Ubuntu 22.04/24.04 et dérivés, quelle que soit la machine
# de compilation. Le pilote SQLite de Qt est un plugin chargé à l'exécution
# (indispensable pour ouvrir la base) et n'a pas de variante t64.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libc6, libstdc++6, libgcc-s1, \
libqt6core6t64 (>= 6.2.0) | libqt6core6 (>= 6.2.0), \
libqt6gui6t64 (>= 6.2.0) | libqt6gui6 (>= 6.2.0), \
libqt6widgets6t64 (>= 6.2.0) | libqt6widgets6 (>= 6.2.0), \
libqt6sql6t64 (>= 6.2.0) | libqt6sql6 (>= 6.2.0), \
libqt6charts6 (>= 6.2.0) | libqt6charts6t64 (>= 6.2.0), \
libqt6sql6-sqlite")
# Nom de fichier conventionnel : grossbuch_<version>-<release>_<arch>.deb.
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

include(CPack)

# --- Tests ------------------------------------------------------------------
add_subdirectory(tests)
