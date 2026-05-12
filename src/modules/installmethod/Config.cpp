/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 MainstreamOS
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 */

#include "Config.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "utils/Variant.h"

namespace InstallMethod
{

QString
choiceToString( Choice c )
{
    switch ( c )
    {
    case Choice::Default:
        return QStringLiteral( "default" );
    case Choice::Custom:
        return QStringLiteral( "custom" );
    case Choice::Developer:
        return QStringLiteral( "developer" );
    case Choice::Gaming:
        return QStringLiteral( "gaming" );
    case Choice::OsOnly:
        return QStringLiteral( "os-only" );
    }
    return QStringLiteral( "default" );
}

Choice
choiceFromString( const QString& s )
{
    const auto trimmed = s.trimmed().toLower();
    if ( trimmed == QStringLiteral( "custom" ) )
    {
        return Choice::Custom;
    }
    if ( trimmed == QStringLiteral( "developer" ) || trimmed == QStringLiteral( "dev" ) )
    {
        return Choice::Developer;
    }
    if ( trimmed == QStringLiteral( "gaming" ) || trimmed == QStringLiteral( "games" ) )
    {
        return Choice::Gaming;
    }
    if ( trimmed == QStringLiteral( "os-only" ) || trimmed == QStringLiteral( "osonly" ) )
    {
        return Choice::OsOnly;
    }
    return Choice::Default;
}

Config::Config( QObject* parent )
    : QObject( parent )
    , m_defaultAppsGroupName( QStringLiteral( "📦 Included Extras" ) )
    , m_developerGroupName( QStringLiteral( "💻 Developer Tools" ) )
    , m_gamingGroupName( QStringLiteral( "🎮 Gaming" ) )
{
    // Seed GlobalStorage with the initial choice so downstream modules
    // (netinstall) have a value to read even if the user never visits
    // this page (e.g. when the page is hidden via settings.conf).
    writeToGlobalStorage();
}

void
Config::setConfigurationMap( const QVariantMap& configurationMap )
{
    const QString defaultChoice
        = Calamares::getString( configurationMap, "defaultChoice", QStringLiteral( "default" ) );
    m_choice = choiceFromString( defaultChoice );

    auto pull = [ & ]( const char* key, QString& target )
    {
        const QString v = Calamares::getString( configurationMap, key, target );
        if ( !v.isEmpty() )
        {
            target = v;
        }
    };
    pull( "defaultAppsGroupName", m_defaultAppsGroupName );
    pull( "developerGroupName",   m_developerGroupName );
    pull( "gamingGroupName",      m_gamingGroupName );

    writeToGlobalStorage();
    emit choiceChanged( m_choice );
}

QString
Config::preselectGroupFor( Choice c ) const
{
    switch ( c )
    {
    case Choice::Default:
        return m_defaultAppsGroupName;
    case Choice::Developer:
        return m_developerGroupName;
    case Choice::Gaming:
        return m_gamingGroupName;
    case Choice::Custom:
    case Choice::OsOnly:
        // Custom: user picks manually on the netinstall page.
        // OsOnly: nothing optional gets installed, so leave preselect
        //         empty and let netinstall's auto-skip path clear all
        //         selections (see NetInstallViewStep::applyInstall…).
        return QString();
    }
    return QString();
}

void
Config::setChoice( Choice c )
{
    if ( c == m_choice )
    {
        return;
    }
    m_choice = c;
    writeToGlobalStorage();
    emit choiceChanged( m_choice );
}

void
Config::setChoiceString( const QString& s )
{
    setChoice( choiceFromString( s ) );
}

void
Config::writeToGlobalStorage() const
{
    auto* gs = Calamares::JobQueue::instance() ? Calamares::JobQueue::instance()->globalStorage() : nullptr;
    if ( !gs )
    {
        return;
    }

    gs->insert( QStringLiteral( "installmethod_choice" ), choiceToString( m_choice ) );

    // Custom is the only path that should actually display the netinstall
    // page — every other choice writes its package outcome from this
    // module and the netinstall ViewStep self-skips on entry. Netinstall
    // honors this in BOTH directions (forward = auto-next past the page,
    // backward = auto-back past the page) so a Back from Summary lands
    // straight on this picker instead of stopping on an empty package
    // list. See NetInstallViewStep::onActivate for the direction logic.
    gs->insert( QStringLiteral( "installmethod_skipnetinstall" ), m_choice != Choice::Custom );

    // Default / Developer / Gaming each pre-check a different netinstall
    // group. Custom and OsOnly both leave the preselect empty — see
    // preselectGroupFor() for the per-choice rationale.
    gs->insert( QStringLiteral( "installmethod_preselect_group" ),
                preselectGroupFor( m_choice ) );
}

}  // namespace InstallMethod
