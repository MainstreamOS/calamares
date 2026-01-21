/* SPDX-FileCopyrightText: 2026 VCPU
   SPDX-License-Identifier: GPL-3.0-or-later */
#include <QBoxLayout>
#include <QFile>
#include <QQmlContext>
#include <QQuickWidget>
#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>
#include <utils/Variant.h>
#include "NetPrivacyJob.h"
#include "NetPrivacyUtils.h"
#include "NetPrivacyViewStep.h"

CALAMARES_PLUGIN_FACTORY_DEFINITION( NetPrivacyViewStepFactory, registerPlugin<NetPrivacy::NetPrivacyViewStep>(); )

namespace NetPrivacy
{

NetPrivacyViewStep::NetPrivacyViewStep( QObject* parent )
    : Calamares::ViewStep( parent )
{
}

NetPrivacyViewStep::~NetPrivacyViewStep()
{
    if ( m_widget && !m_widget->parent() )
        m_widget->deleteLater();
}

QString NetPrivacyViewStep::prettyName() const { return tr( "Network Privacy" ); }

bool NetPrivacyViewStep::isNextEnabled() const
{
    if ( m_macPolicy == MacPolicy::Fixed )
        return Utils::isValidMac( m_macAddress );
    if ( m_macPolicy == MacPolicy::Vendor && m_selectedVendor.isEmpty() )
        return false;
    return true;
}

QWidget* NetPrivacyViewStep::widget()
{
    if ( m_widget ) return m_widget;

    m_widget = new QWidget;
    auto* layout = new QVBoxLayout( m_widget );
    layout->setContentsMargins( 0, 0, 0, 0 );

    auto* qw = new QQuickWidget( m_widget );
    qw->setClearColor( Qt::transparent );
    qw->setAttribute( Qt::WA_AlwaysStackOnTop );
    qw->setAttribute( Qt::WA_TranslucentBackground );
    qw->setResizeMode( QQuickWidget::SizeRootObjectToView );
    qw->rootContext()->setContextProperty( QStringLiteral( "config" ), this );
    qmlRegisterUncreatableType<NetPrivacyViewStep>(
        "io.calamares.netprivacy", 1, 0, "NetPrivacy", QString() );
    qw->setSource( QUrl( QStringLiteral( "qrc:/netprivacy/View.qml" ) ) );

    if ( qw->status() == QQuickWidget::Error )
        for ( const auto& e : qw->errors() )
            cError() << "QML:" << e.toString();

    layout->addWidget( qw );
    return m_widget;
}

void NetPrivacyViewStep::setConfigurationMap( const QVariantMap& cfg )
{
    m_config.vendors.clear();
    m_config.writeNetworkManagerConfig  = Calamares::getBool( cfg, "writeNetworkManagerConfig", true );
    m_config.writeSystemdNetworkdConfig = Calamares::getBool( cfg, "writeSystemdNetworkdConfig", true );
    m_config.perConnectionRandom        = Calamares::getBool( cfg, "perConnectionRandom", false );

    if ( cfg.contains( "vendors" ) )
    {
        for ( const auto& v : cfg.value( "vendors" ).toList() )
        {
            auto m = v.toMap();
            QString id   = m.value( "id" ).toString();
            QString name = m.value( "name" ).toString();
            QString oui  = m.value( "oui" ).toString();

            if ( id.isEmpty() || name.isEmpty() || !Utils::isValidOui( oui ) )
            {
                cWarning() << "NetPrivacy: bad vendor:" << id;
                continue;
            }

            m_config.vendors.append( { id, name, oui } );
        }
    }

    if ( m_config.vendors.isEmpty() )
    {
        cWarning() << "NetPrivacy: no vendors in config, using defaults";
        m_config.vendors = {
            { QStringLiteral( "intel" ),   QStringLiteral( "Intel Corporation" ),   QStringLiteral( "00:1B:21" ) },
            { QStringLiteral( "realtek" ), QStringLiteral( "Realtek Semiconductor" ), QStringLiteral( "00:E0:4C" ) },
            { QStringLiteral( "generic" ), QStringLiteral( "Generic (Local Admin)" ), QStringLiteral( "02:00:00" ) }
        };
    }

    setMacPolicyInt( Calamares::getInteger( cfg, "macPolicy", 0 ) );
    setIpv6ModeInt( Calamares::getInteger( cfg, "ipv6Mode", 0 ) );
    setMacAddress( cfg.value( "macAddress" ).toString() );

    QString sv = cfg.value( "selectedVendor" ).toString();
    if ( !sv.isEmpty() ) setSelectedVendor( sv );
    if ( m_selectedVendor.isEmpty() && !m_config.vendors.isEmpty() )
        m_selectedVendor = m_config.vendors.first().id;

    Q_EMIT vendorListChanged();
    Q_EMIT perConnectionRandomChanged();
}

Calamares::JobList NetPrivacyViewStep::jobs() const
{
    Calamares::JobList list;
    if ( m_macPolicy == MacPolicy::Disabled && m_ipv6Mode == Ipv6Mode::Standard )
        return list;

    QString oui;
    if ( m_macPolicy == MacPolicy::Vendor )
        for ( const auto& v : m_config.vendors )
            if ( v.id == m_selectedVendor ) { oui = v.oui; break; }

    list.append( Calamares::job_ptr( new NetPrivacyJob(
        m_macPolicy, m_macAddress, oui, m_ipv6Mode,
        m_config.writeNetworkManagerConfig,
        m_config.writeSystemdNetworkdConfig,
        m_config.perConnectionRandom ) ) );
    return list;
}

void NetPrivacyViewStep::setMacPolicyInt( int p )
{
    if ( p < 0 || p > 3 ) return;
    auto pol = static_cast<MacPolicy>( p );
    if ( m_macPolicy != pol )
    {
        m_macPolicy = pol;
        Q_EMIT macPolicyChanged();
        Q_EMIT nextStatusChanged( isNextEnabled() );
    }
}

void NetPrivacyViewStep::setMacAddress( const QString& addr )
{
    QString clean = addr.trimmed().toUpper();
    if ( !clean.isEmpty() && !Utils::isValidMac( clean ) ) return;
    if ( m_macAddress != clean )
    {
        m_macAddress = clean;
        Q_EMIT macAddressChanged();
        Q_EMIT nextStatusChanged( isNextEnabled() );
    }
}

void NetPrivacyViewStep::setSelectedVendor( const QString& v )
{
    if ( v.isEmpty() ) return;
    bool ok = false;
    for ( const auto& e : m_config.vendors )
        if ( e.id == v ) { ok = true; break; }
    if ( !ok ) return;
    if ( m_selectedVendor != v )
    {
        m_selectedVendor = v;
        Q_EMIT selectedVendorChanged();
        Q_EMIT macPolicyChanged();
        Q_EMIT nextStatusChanged( isNextEnabled() );
    }
}

void NetPrivacyViewStep::setIpv6ModeInt( int m )
{
    if ( m < 0 || m > 2 ) return;
    auto mode = static_cast<Ipv6Mode>( m );
    if ( m_ipv6Mode != mode )
    {
        m_ipv6Mode = mode;
        Q_EMIT ipv6ModeChanged();
    }
}

void NetPrivacyViewStep::setPerConnectionRandom( bool r )
{
    if ( m_config.perConnectionRandom != r )
    {
        m_config.perConnectionRandom = r;
        Q_EMIT perConnectionRandomChanged();
    }
}

QVariantList NetPrivacyViewStep::vendorList() const
{
    QVariantList l;
    for ( const auto& v : m_config.vendors )
    {
        QVariantMap m;
        m["id"]   = v.id;
        m["name"] = v.name;
        m["oui"]  = v.oui;
        l.append( m );
    }
    return l;
}

QString NetPrivacyViewStep::generatePreviewMac() const
{
    if ( m_macPolicy == MacPolicy::Random )
        return Utils::generateLocalMac();
    if ( m_macPolicy == MacPolicy::Vendor )
        for ( const auto& v : m_config.vendors )
            if ( v.id == m_selectedVendor )
                return Utils::generateVendorMacSafe( v.oui );
    return QString();
}

bool NetPrivacyViewStep::isVirtualMachine() const
{
    QFile dmi( QStringLiteral( "/sys/class/dmi/id/product_name" ) );
    if ( dmi.open( QIODevice::ReadOnly ) )
    {
        QString name = QString::fromUtf8( dmi.readAll() ).toLower().trimmed();
        if ( name.contains( "virtualbox" ) || name.contains( "vmware" )
             || name.contains( "qemu" ) || name.contains( "kvm" )
             || name.contains( "virtual machine" ) || name.contains( "hyper-v" ) )
            return true;
    }

    QFile vendor( QStringLiteral( "/sys/class/dmi/id/sys_vendor" ) );
    if ( vendor.open( QIODevice::ReadOnly ) )
    {
        QString v = QString::fromUtf8( vendor.readAll() ).toLower().trimmed();
        if ( v.contains( "vmware" ) || v.contains( "innotek" ) || v.contains( "oracle" )
             || v.contains( "qemu" ) || v.contains( "microsoft corporation" )
             || v.contains( "xen" ) || v.contains( "bochs" ) || v.contains( "parallels" ) )
            return true;
    }

    return false;
}

}
