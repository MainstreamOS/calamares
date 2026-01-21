/* SPDX-FileCopyrightText: 2026 VCPU
   SPDX-License-Identifier: GPL-3.0-or-later */
#include <QDir>
#include <GlobalStorage.h>
#include <JobQueue.h>
#include <utils/Logger.h>
#include "NetPrivacyJob.h"
#include "NetPrivacyUtils.h"

namespace NetPrivacy
{

NetPrivacyJob::NetPrivacyJob( MacPolicy macPolicy,
                              const QString& macAddress,
                              const QString& vendorOUI,
                              Ipv6Mode ipv6Mode,
                              bool writeNM,
                              bool writeSystemd,
                              bool perConnRandom,
                              QObject* parent )
    : Calamares::CppJob( parent )
    , m_macPolicy( macPolicy )
    , m_ipv6Mode( ipv6Mode )
    , m_macAddress( macAddress )
    , m_vendorOUI( macPolicy == MacPolicy::Vendor && !Utils::isValidOui( vendorOUI ) 
                   ? Utils::kDefaultOui : vendorOUI )
    , m_writeNM( writeNM )
    , m_writeSystemd( writeSystemd )
    , m_perConnRandom( perConnRandom )
{
}

NetPrivacyJob::~NetPrivacyJob() {}

QString NetPrivacyJob::prettyName() const { return tr( "Configure Network Privacy" ); }

Calamares::JobResult NetPrivacyJob::exec()
{
    if ( m_macPolicy == MacPolicy::Fixed && !Utils::isValidMac( m_macAddress ) )
        return Calamares::JobResult::error( tr( "Error" ), tr( "Invalid MAC: %1" ).arg( m_macAddress ) );

    if ( m_macPolicy == MacPolicy::Fixed )
        m_execMac = m_macAddress;
    else if ( !m_perConnRandom )
        m_execMac = ( m_macPolicy == MacPolicy::Random )
            ? Utils::generateLocalMac()
            : Utils::generateVendorMacSafe( m_vendorOUI );

    QString testRoot = qEnvironmentVariable( "NETPRIVACY_TEST_ROOT" );
    QString root;
    
    if ( !testRoot.isEmpty() )
    {
        root = testRoot;
        QDir().mkpath( root );
    }
    else
    {
        auto* gs = Calamares::JobQueue::instance()->globalStorage();
        root = gs->value( "rootMountPoint" ).toString();
        if ( root.isEmpty() )
            return Calamares::JobResult::error( tr( "Error" ), tr( "No root mount point" ) );
    }

    root = QDir::cleanPath( root );
    if ( !QDir::isAbsolutePath( root ) )
        return Calamares::JobResult::error( tr( "Error" ), tr( "Invalid path: %1" ).arg( root ) );

    if ( m_macPolicy != MacPolicy::Disabled )
    {
        if ( auto r = writeMacConfig( root ); !r ) return r;
        if ( auto r = writeSystemdLinkConfig( root ); !r ) return r;
    }

    if ( m_ipv6Mode != Ipv6Mode::Standard )
    {
        if ( auto r = writeIpv6Config( root ); !r ) return r;
    }

    return Calamares::JobResult::ok();
}

Calamares::JobResult NetPrivacyJob::writeMacConfig( const QString& root ) const
{
    if ( !m_writeNM ) return Calamares::JobResult::ok();

    QString c = "# Calamares NetPrivacy - MAC Randomization\n\n";

    if ( m_macPolicy == MacPolicy::Random || m_macPolicy == MacPolicy::Vendor )
    {
        c += "[device]\nwifi.scan-rand-mac-address=yes\n\n[connection]\n";
        if ( m_perConnRandom )
        {
            c += "wifi.cloned-mac-address=random\n";
            c += "ethernet.cloned-mac-address=random\n";
        }
        else
        {
            c += QString( "wifi.cloned-mac-address=%1\n" ).arg( m_execMac );
            c += QString( "ethernet.cloned-mac-address=%1\n" ).arg( m_execMac );
        }
    }
    else if ( m_macPolicy == MacPolicy::Fixed )
    {
        c += "[connection]\n";
        c += QString( "wifi.cloned-mac-address=%1\n" ).arg( m_execMac );
        c += QString( "ethernet.cloned-mac-address=%1\n" ).arg( m_execMac );
    }

    return Utils::writeAtomicFile( Utils::nmMacConfPath( root ), c );
}

Calamares::JobResult NetPrivacyJob::writeSystemdLinkConfig( const QString& root ) const
{
    if ( !m_writeSystemd ) return Calamares::JobResult::ok();

    QString c = "# Calamares NetPrivacy - MAC Randomization\n\n";
    c += "[Match]\nOriginalName=*\n";
    c += "Type=!loopback !bridge !bond !vlan !tunnel\n";
    c += "Name=!docker* !veth* !br-* !virbr* !lxc*\n\n";
    c += "[Link]\n";

    if ( m_macPolicy == MacPolicy::Fixed )
        c += QString( "MACAddress=%1\n" ).arg( m_execMac );
    else if ( m_perConnRandom )
        c += "MACAddressPolicy=random\n";
    else
        c += QString( "MACAddress=%1\n" ).arg( m_execMac );

    return Utils::writeAtomicFile( Utils::systemdLinkPath( root ), c );
}

Calamares::JobResult NetPrivacyJob::writeIpv6Config( const QString& root ) const
{
    if ( m_ipv6Mode == Ipv6Mode::Ipv6Disabled )
    {
        QString c = "# Calamares NetPrivacy - IPv6 disabled\n";
        c += "net.ipv6.conf.all.disable_ipv6 = 1\n";
        c += "net.ipv6.conf.default.disable_ipv6 = 1\n";
        c += "net.ipv6.conf.lo.disable_ipv6 = 1\n";
        return Utils::writeAtomicFile( Utils::sysctlDisableIpv6Path( root ), c );
    }

    if ( m_ipv6Mode == Ipv6Mode::Privacy )
    {
        if ( m_writeNM )
        {
            QString c = "# Calamares NetPrivacy - IPv6 Privacy Extensions\n\n[connection]\nipv6.ip6-privacy=2\n";
            if ( auto r = Utils::writeAtomicFile( Utils::nmIpv6ConfPath( root ), c ); !r )
                return r;
        }

        if ( m_writeSystemd )
        {
            QString c = "# Calamares NetPrivacy - IPv6 Privacy Extensions\n\n";
            c += "[Match]\nName=*\n\n[Network]\nIPv6PrivacyExtensions=prefer-temporary\n";
            if ( auto r = Utils::writeAtomicFile( Utils::networkdIpv6ConfPath( root ), c ); !r )
                return r;
        }
    }

    return Calamares::JobResult::ok();
}

}
