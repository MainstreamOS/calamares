/* SPDX-FileCopyrightText: 2026 VCPU
   SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NETPRIVACYTYPES_H
#define NETPRIVACYTYPES_H

#include <QObject>
#include <QString>
#include <QVector>

namespace NetPrivacy
{

enum class MacPolicy { Disabled = 0, Random, Vendor, Fixed };
enum class Ipv6Mode  { Standard = 0, Privacy, Ipv6Disabled };

struct VendorEntry { QString id, name, oui; };

struct Config
{
    QVector<VendorEntry> vendors;
    bool writeNetworkManagerConfig   = true;
    bool writeSystemdNetworkdConfig  = true;
    bool perConnectionRandom         = false;
};

}

Q_DECLARE_METATYPE( NetPrivacy::MacPolicy )
Q_DECLARE_METATYPE( NetPrivacy::Ipv6Mode )

#endif
