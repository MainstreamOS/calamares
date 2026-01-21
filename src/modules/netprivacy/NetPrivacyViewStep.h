/* SPDX-FileCopyrightText: 2026 VCPU
   SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NETPRIVACYVIEWSTEP_H
#define NETPRIVACYVIEWSTEP_H

#include <viewpages/ViewStep.h>
#include <utils/PluginFactory.h>
#include "NetPrivacyTypes.h"
#include <QObject>
#include <QVariantList>

class QWidget;

namespace NetPrivacy
{

class NetPrivacyViewStep : public Calamares::ViewStep
{
    Q_OBJECT

    Q_PROPERTY( int macPolicy READ macPolicyInt WRITE setMacPolicyInt NOTIFY macPolicyChanged )
    Q_PROPERTY( QString macAddress READ macAddress WRITE setMacAddress NOTIFY macAddressChanged )
    Q_PROPERTY( QString selectedVendor READ selectedVendor WRITE setSelectedVendor NOTIFY selectedVendorChanged )
    Q_PROPERTY( int ipv6Mode READ ipv6ModeInt WRITE setIpv6ModeInt NOTIFY ipv6ModeChanged )
    Q_PROPERTY( bool perConnectionRandom READ perConnectionRandom WRITE setPerConnectionRandom NOTIFY perConnectionRandomChanged )
    Q_PROPERTY( QVariantList vendorList READ vendorList NOTIFY vendorListChanged )
    Q_PROPERTY( QString currentPreviewMac READ generatePreviewMac NOTIFY macPolicyChanged )
    Q_PROPERTY( bool isVirtualMachine READ isVirtualMachine CONSTANT )

public:
    explicit NetPrivacyViewStep( QObject* parent = nullptr );
    ~NetPrivacyViewStep() override;

    QString prettyName() const override;
    QWidget* widget() override;
    void setConfigurationMap( const QVariantMap& cfg ) override;
    Calamares::JobList jobs() const override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override { return true; }
    bool isAtBeginning() const override { return true; }
    bool isAtEnd() const override       { return true; }

    int macPolicyInt() const         { return static_cast<int>( m_macPolicy ); }
    QString macAddress() const       { return m_macAddress; }
    QString selectedVendor() const   { return m_selectedVendor; }
    int ipv6ModeInt() const          { return static_cast<int>( m_ipv6Mode ); }
    bool perConnectionRandom() const { return m_config.perConnectionRandom; }

    void setMacPolicyInt( int p );
    void setMacAddress( const QString& addr );
    void setSelectedVendor( const QString& v );
    void setIpv6ModeInt( int m );
    void setPerConnectionRandom( bool r );

    QVariantList vendorList() const;
    QString generatePreviewMac() const;
    bool isVirtualMachine() const;

Q_SIGNALS:
    void macPolicyChanged();
    void macAddressChanged();
    void selectedVendorChanged();
    void ipv6ModeChanged();
    void vendorListChanged();
    void perConnectionRandomChanged();

private:
    Config    m_config;
    MacPolicy m_macPolicy = MacPolicy::Disabled;
    Ipv6Mode  m_ipv6Mode  = Ipv6Mode::Standard;
    QString   m_macAddress;
    QString   m_selectedVendor;
    QWidget*  m_widget = nullptr;
};

}

CALAMARES_PLUGIN_FACTORY_DECLARATION( NetPrivacyViewStepFactory )

#endif
