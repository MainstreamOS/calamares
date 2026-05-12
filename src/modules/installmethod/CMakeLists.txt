# === This file is part of Calamares - <https://calamares.io> ===
#
#   SPDX-FileCopyrightText: 2026 MainstreamOS
#   SPDX-License-Identifier: BSD-2-Clause

find_package(${qtname} ${QT_VERSION} CONFIG REQUIRED Svg)

calamares_add_plugin(installmethod
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        Config.cpp
        Config.h
        InstallMethodPage.cpp
        InstallMethodPage.h
        InstallMethodViewStep.cpp
        InstallMethodViewStep.h
    RESOURCES
        installmethod.qrc
    LINK_PRIVATE_LIBRARIES
        ${qtname}::Svg
    SHARED_LIB
)
