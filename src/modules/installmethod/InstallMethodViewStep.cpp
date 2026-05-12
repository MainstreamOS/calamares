/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 MainstreamOS
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 */

#include "InstallMethodViewStep.h"

#include "Config.h"
#include "InstallMethodPage.h"

#include "utils/Logger.h"

CALAMARES_PLUGIN_FACTORY_DEFINITION( InstallMethodViewStepFactory, registerPlugin< InstallMethodViewStep >(); )

InstallMethodViewStep::InstallMethodViewStep( QObject* parent )
    : Calamares::ViewStep( parent )
    , m_config( new InstallMethod::Config( this ) )
    , m_widget( new InstallMethod::InstallMethodPage( m_config ) )
{
}

InstallMethodViewStep::~InstallMethodViewStep()
{
    if ( m_widget && m_widget->parent() == nullptr )
    {
        m_widget->deleteLater();
    }
}

QString
InstallMethodViewStep::prettyName() const
{
    return tr( "Get Started", "@title" );
}

QString
InstallMethodViewStep::prettyStatus() const
{
    // Surface the user's pick on the Summary page so they can see at a
    // glance whether the installer is going to bring along the curated
    // app set, send them through netinstall, install a themed bundle,
    // or strip it back to the bare OS.
    if ( !m_config )
    {
        return QString();
    }
    switch ( m_config->choice() )
    {
    case InstallMethod::Choice::Default:
        return tr( "Default Apps — install the curated %1 group automatically." )
            .arg( m_config->defaultAppsGroupName() );
    case InstallMethod::Choice::Custom:
        return tr( "Customize — you will pick packages on the next page." );
    case InstallMethod::Choice::Developer:
        return tr( "Developer — install the %1 group automatically." )
            .arg( m_config->developerGroupName() );
    case InstallMethod::Choice::Gaming:
        return tr( "Gaming — install the %1 group automatically." )
            .arg( m_config->gamingGroupName() );
    case InstallMethod::Choice::OsOnly:
        return tr( "OS Only — no optional apps will be installed." );
    }
    return QString();
}

QWidget*
InstallMethodViewStep::widget()
{
    return m_widget;
}

bool
InstallMethodViewStep::isNextEnabled() const
{
    // Config seeds a default choice in its constructor, so the user can
    // always proceed even without explicitly clicking a card.
    return true;
}

bool
InstallMethodViewStep::isBackEnabled() const
{
    return true;
}

bool
InstallMethodViewStep::isAtBeginning() const
{
    return true;
}

bool
InstallMethodViewStep::isAtEnd() const
{
    return true;
}

Calamares::JobList
InstallMethodViewStep::jobs() const
{
    // No exec-phase work — this module only writes to GlobalStorage,
    // which downstream modules (netinstall, packages) consume.
    return {};
}

void
InstallMethodViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
}
