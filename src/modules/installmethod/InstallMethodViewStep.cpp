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
