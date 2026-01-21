# === This file is part of Calamares - <https://calamares.io> ===
#
#   SPDX-FileCopyrightText: 2026 VCPU
#   SPDX-License-Identifier: GPL-3.0-or-later
#

if(NOT WITH_QML)
    calamares_skip_module( "netprivacy (QML is not supported in this build)" )
    return()
endif()

calamares_add_plugin(netprivacy
    TYPE viewmodule
    EXPORT_MACRO PLUGINDLLEXPORT_PRO
    SOURCES
        NetPrivacyJob.cpp
        NetPrivacyViewStep.cpp
    RESOURCES
        netprivacy.qrc
    LINK_PRIVATE_LIBRARIES
        ${qtname}::Quick
        ${qtname}::QuickWidgets
    SHARED_LIB
)
