/* SPDX-FileCopyrightText: 2026 VCPU
   SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NETPRIVACYUTILS_H
#define NETPRIVACYUTILS_H

#include <QDir>
#include <QFileInfo>
#include <QLatin1Char>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QString>
#include <QTextStream>
#include <Job.h>

namespace NetPrivacy { namespace Utils {

inline const QString kNmMacConf       = QStringLiteral( "etc/NetworkManager/conf.d/80-calamares-mac.conf" );
inline const QString kNmIpv6Conf      = QStringLiteral( "etc/NetworkManager/conf.d/80-calamares-ipv6.conf" );
inline const QString kSystemdLink     = QStringLiteral( "etc/systemd/network/80-calamares-mac.link" );
inline const QString kNetworkdIpv6    = QStringLiteral( "etc/systemd/network/80-calamares-ipv6.network" );
inline const QString kSysctlIpv6      = QStringLiteral( "etc/sysctl.d/99-calamares-ipv6.conf" );
inline const QString kDefaultOui      = QStringLiteral( "02:00:00" );

inline const QRegularExpression kMacRx( QStringLiteral( "^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$" ) );
inline const QRegularExpression kOuiRx( QStringLiteral( "^[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}:[0-9A-Fa-f]{2}$" ) );

inline bool isValidMac( const QString& mac ) { return kMacRx.match( mac ).hasMatch(); }
inline bool isValidOui( const QString& oui ) { return kOuiRx.match( oui ).hasMatch(); }

inline quint8 rndByte() { return static_cast<quint8>( QRandomGenerator::global()->bounded( 256 ) ); }

inline QString generateLocalMac()
{
    quint8 b0 = static_cast<quint8>( ( rndByte() & 0xFE ) | 0x02 );
    return QStringLiteral( "%1:%2:%3:%4:%5:%6" )
        .arg( b0, 2, 16, QLatin1Char( '0' ) )
        .arg( rndByte(), 2, 16, QLatin1Char( '0' ) )
        .arg( rndByte(), 2, 16, QLatin1Char( '0' ) )
        .arg( rndByte(), 2, 16, QLatin1Char( '0' ) )
        .arg( rndByte(), 2, 16, QLatin1Char( '0' ) )
        .arg( rndByte(), 2, 16, QLatin1Char( '0' ) )
        .toUpper();
}

inline QString generateVendorMac( const QString& oui )
{
    return ( oui + QStringLiteral( ":%1:%2:%3" )
        .arg( rndByte(), 2, 16, QLatin1Char( '0' ) )
        .arg( rndByte(), 2, 16, QLatin1Char( '0' ) )
        .arg( rndByte(), 2, 16, QLatin1Char( '0' ) ) ).toUpper();
}

inline QString generateVendorMacSafe( const QString& oui )
{
    return generateVendorMac( isValidOui( oui ) ? oui : kDefaultOui );
}

inline QString nmMacConfPath( const QString& root )       { return QDir( root ).filePath( kNmMacConf ); }
inline QString nmIpv6ConfPath( const QString& root )      { return QDir( root ).filePath( kNmIpv6Conf ); }
inline QString systemdLinkPath( const QString& root )     { return QDir( root ).filePath( kSystemdLink ); }
inline QString networkdIpv6ConfPath( const QString& root ){ return QDir( root ).filePath( kNetworkdIpv6 ); }
inline QString sysctlDisableIpv6Path( const QString& root){ return QDir( root ).filePath( kSysctlIpv6 ); }

inline Calamares::JobResult writeAtomicFile( const QString& path, const QString& content )
{
    QFileInfo info( path );
    if ( !info.dir().exists() && !info.dir().mkpath( "." ) )
        return Calamares::JobResult::error(
            QObject::tr( "File Error" ),
            QObject::tr( "Failed to create directory: %1" ).arg( info.dir().path() ) );

    QSaveFile f( path );
    if ( !f.open( QIODevice::WriteOnly | QIODevice::Text ) )
        return Calamares::JobResult::error(
            QObject::tr( "File Error" ),
            QObject::tr( "Failed to open file for writing: %1" ).arg( path ) );

    QTextStream out( &f );
    out << content;
    if ( out.status() != QTextStream::Ok || !f.commit() )
        return Calamares::JobResult::error(
            QObject::tr( "File Error" ),
            QObject::tr( "Failed to write file: %1" ).arg( path ) );

    return Calamares::JobResult::ok();
}

}}

#endif