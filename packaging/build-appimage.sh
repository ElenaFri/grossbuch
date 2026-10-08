#!/usr/bin/env bash
# Construit une AppImage portable de grossbuch (voir docs/adr/0021).
#
# L'AppImage est un format d'installation complémentaire du .deb : un fichier
# unique, exécutable sur la plupart des distributions Linux, qui embarque Qt et
# ses greffons. Elle sert de secours hors écosystème Debian et ne remplace pas
# le .deb, format principal et mieux intégré au bureau.
#
# Prérequis de compilation identiques au .deb (cmake, compilateur, qt6-base-dev,
# qt6-charts-dev, libqt6sql6-sqlite). Les outils linuxdeploy sont téléchargés
# automatiquement dans build/ (ignoré par Git) s'ils sont absents. curl requis.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${ROOT}/build"
APPDIR="${BUILD_DIR}/AppDir"
TOOLS_DIR="${BUILD_DIR}/appimage-tools"

# 1. Compiler en Release.
cmake -S "${ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" -j

# 2. Installer dans un AppDir, préfixe /usr (binaire, .desktop, icône).
rm -rf "${APPDIR}"
DESTDIR="${APPDIR}" cmake --install "${BUILD_DIR}" --prefix /usr

# 3. Récupérer linuxdeploy et son greffon Qt (si absents).
mkdir -p "${TOOLS_DIR}"
LINUXDEPLOY="${TOOLS_DIR}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="${TOOLS_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"
if [ ! -f "${LINUXDEPLOY}" ]; then
    curl -fsSL -o "${LINUXDEPLOY}" \
        https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fi
if [ ! -f "${LINUXDEPLOY_QT}" ]; then
    curl -fsSL -o "${LINUXDEPLOY_QT}" \
        https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
fi
chmod +x "${LINUXDEPLOY}" "${LINUXDEPLOY_QT}"

# Le greffon Qt localise Qt via qmake : on le lui indique explicitement car,
# sur Debian/Ubuntu, le binaire s'appelle qmake6.
if [ -z "${QMAKE:-}" ]; then
    if command -v qmake6 >/dev/null 2>&1; then
        QMAKE="$(command -v qmake6)"
    elif [ -x /usr/lib/qt6/bin/qmake6 ]; then
        QMAKE=/usr/lib/qt6/bin/qmake6
    fi
fi
export QMAKE

# Inclut de force le pilote SQL SQLite de Qt, indispensable pour ouvrir la base
# et que le greffon Qt n'embarque pas de lui-même.
export EXTRA_QT_PLUGINS="sqldrivers"
# Permet aux outils AppImage de tourner sans FUSE (utile en CI).
export APPIMAGE_EXTRACT_AND_RUN=1
export OUTPUT="grossbuch-x86_64.AppImage"

cd "${BUILD_DIR}"
"${LINUXDEPLOY}" --appdir "${APPDIR}" \
    --plugin qt \
    --output appimage \
    --desktop-file "${APPDIR}/usr/share/applications/grossbuch.desktop" \
    --icon-file "${APPDIR}/usr/share/icons/hicolor/scalable/apps/grossbuch.svg"

echo "AppImage produite : ${BUILD_DIR}/${OUTPUT}"
