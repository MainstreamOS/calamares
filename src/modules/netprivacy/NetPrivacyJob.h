/* SPDX-FileCopyrightText: 2026 VCPU
   SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NETPRIVACYJOB_H
#define NETPRIVACYJOB_H

#include <CppJob.h>
#include "NetPrivacyTypes.h"

namespace NetPrivacy
{

class NetPrivacyJob : public Calamares::CppJob
{
    Q_OBJECT

public:
    NetPrivacyJob( MacPolicy macPolicy,
                   const QString& macAddress,
                   const QString& vendorOUI,
                   Ipv6Mode ipv6Mode,
                   bool writeNM,
                   bool writeSystemd,
                   bool perConnRandom,
                   QObject* parent = nullptr );
    ~NetPrivacyJob() override;

    QString prettyName() const override;
    Calamares::JobResult exec() override;

private:
    Calamares::JobResult writeMacConfig( const QString& root ) const;
    Calamares::JobResult writeSystemdLinkConfig( const QString& root ) const;
    Calamares::JobResult writeIpv6Config( const QString& root ) const;

    MacPolicy m_macPolicy;
    Ipv6Mode  m_ipv6Mode;
    QString   m_macAddress;
    QString   m_vendorOUI;
    QString   m_execMac;
    bool      m_writeNM;
    bool      m_writeSystemd;
    bool      m_perConnRandom;
};

}

#endif
