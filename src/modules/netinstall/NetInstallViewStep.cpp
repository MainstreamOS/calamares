/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2016 Luca Giambonini <almack@chakraos.org>
 *   SPDX-FileCopyrightText: 2016 Lisa Vitolo <shainer@chakraos.org>
 *   SPDX-FileCopyrightText: 2017 Kyle Robbertze  <krobbertze@gmail.com>
 *   SPDX-FileCopyrightText: 2017-2018 2020, Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "NetInstallViewStep.h"

#include "NetInstallPage.h"
#include "PackageModel.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "ViewManager.h"

#include <QTimer>

CALAMARES_PLUGIN_FACTORY_DEFINITION( NetInstallViewStepFactory, registerPlugin< NetInstallViewStep >(); )

NetInstallViewStep::NetInstallViewStep( QObject* parent )
    : Calamares::ViewStep( parent )
    , m_widget( new NetInstallPage( &m_config ) )
    , m_nextEnabled( false )
    , m_pendingSkip( false )
    , m_pendingSkipBackward( false )
    , m_lastSeenStepIndex( -1 )
{
    connect( &m_config, &Config::statusReady, this, &NetInstallViewStep::nextIsReady );

    // Track every step transition so onActivate() can tell whether the
    // user just walked forward into this page or backed into it from
    // Summary. ViewManager emits currentStepChanged AFTER the new
    // step's onActivate has already run, so when our own onActivate
    // fires this slot has not yet been called for the transition that
    // brought us here — m_lastSeenStepIndex still holds the index of
    // the step the user was on before. Subtle but reliable; see
    // ViewManager::next() / ::back() ordering in libcalamaresui.
    if ( auto* vm = Calamares::ViewManager::instance() )
    {
        connect( vm,
                 &Calamares::ViewManager::currentStepChanged,
                 this,
                 [ this ]()
                 {
                     if ( auto* mgr = Calamares::ViewManager::instance() )
                     {
                         m_lastSeenStepIndex = mgr->currentStepIndex();
                     }
                 } );
    }
}


NetInstallViewStep::~NetInstallViewStep()
{
    if ( m_widget && m_widget->parent() == nullptr )
    {
        m_widget->deleteLater();
    }
}


QString
NetInstallViewStep::prettyName() const
{
    return m_config.sidebarLabel();

#if defined( TABLE_OF_TRANSLATIONS )
    __builtin_unreachable();
    // This is a table of "standard" labels for this module. If you use them
    // in the label: sidebar: section of the config file, the existing
    // translations can be used.
    //
    // These translations still live here, even though the lookup
    // code is in the Config class.
    tr( "Package selection" );
    tr( "Office software" );
    tr( "Office package" );
    tr( "Browser software" );
    tr( "Browser package" );
    tr( "Web browser" );
    tr( "Kernel", "label for netinstall module, Linux kernel" );
    tr( "Services", "label for netinstall module, system services" );
    tr( "Login", "label for netinstall module, choose login manager" );
    tr( "Desktop", "label for netinstall module, choose desktop environment" );
    tr( "Applications" );
    tr( "Communication", "label for netinstall module" );
    tr( "Development", "label for netinstall module" );
    tr( "Office", "label for netinstall module" );
    tr( "Multimedia", "label for netinstall module" );
    tr( "Internet", "label for netinstall module" );
    tr( "Theming", "label for netinstall module" );
    tr( "Gaming", "label for netinstall module" );
    tr( "Utilities", "label for netinstall module" );
#endif
}


QWidget*
NetInstallViewStep::widget()
{
    return m_widget;
}


bool
NetInstallViewStep::isNextEnabled() const
{
    return !m_config.required() || m_nextEnabled;
}


bool
NetInstallViewStep::isBackEnabled() const
{
    return true;
}


bool
NetInstallViewStep::isAtBeginning() const
{
    return true;
}


bool
NetInstallViewStep::isAtEnd() const
{
    return true;
}


Calamares::JobList
NetInstallViewStep::jobs() const
{
    return Calamares::JobList();
}


void
NetInstallViewStep::onActivate()
{
    // Honor a self-skip signal from the installmethod module: when the
    // user picked "Default Apps" or "OS Only" on the preceding page,
    // GlobalStorage["installmethod_skipnetinstall"] is true, and we
    // bounce past this page in WHICHEVER direction the user was
    // navigating. Forward = land on Summary (with preselect / clear
    // applied). Backward = land back on installmethod, so a Back from
    // Summary walks straight to the picker rather than parking on an
    // empty netinstall page that the user can't actually edit.
    if ( installmethodWantsSkip() )
    {
        auto* vm           = Calamares::ViewManager::instance();
        const int myIndex  = vm ? vm->viewSteps().indexOf( this ) : -1;
        // Backward iff the step we last saw current was past us.
        // m_lastSeenStepIndex is the slot-tracked previous value; see
        // the connect() in the constructor for the timing rationale.
        const bool backward = ( m_lastSeenStepIndex > myIndex );

        if ( m_nextEnabled || backward )
        {
            // Backward path doesn't need the package model to be
            // loaded — we're just bouncing through — so we run it
            // immediately even before statusReady.
            applyInstallMethodChoiceAndAdvance( backward );
        }
        else
        {
            // Forward path needs the YAML loaded so the preselect
            // group can be applied. Defer until statusReady fires.
            m_pendingSkip         = true;
            m_pendingSkipBackward = false;
        }
        return;
    }

    m_widget->onActivate();
}

void
NetInstallViewStep::onLeave()
{
    m_config.finalizeGlobalStorage( moduleInstanceKey() );
}

void
NetInstallViewStep::nextIsReady()
{
    m_nextEnabled = true;
    emit nextStatusChanged( true );

    if ( m_pendingSkip )
    {
        m_pendingSkip = false;
        applyInstallMethodChoiceAndAdvance( m_pendingSkipBackward );
    }
}

bool
NetInstallViewStep::installmethodWantsSkip() const
{
    auto* gs = Calamares::JobQueue::instance() ? Calamares::JobQueue::instance()->globalStorage() : nullptr;
    if ( !gs )
    {
        return false;
    }
    return gs->contains( QStringLiteral( "installmethod_skipnetinstall" ) )
        && gs->value( QStringLiteral( "installmethod_skipnetinstall" ) ).toBool();
}

void
NetInstallViewStep::applyInstallMethodChoiceAndAdvance( bool backward )
{
    auto* gs = Calamares::JobQueue::instance() ? Calamares::JobQueue::instance()->globalStorage() : nullptr;
    if ( !gs )
    {
        return;
    }

    if ( !backward )
    {
        // Only apply package-model side effects on the forward pass.
        // Going backward through here we're just bouncing through to
        // installmethod, and the selections were already set when the
        // forward auto-skip ran (or by the user, if they came from a
        // Custom visit). Touching the model on the way back would
        // overwrite their picks.
        const QString preselectGroup
            = gs->value( QStringLiteral( "installmethod_preselect_group" ) ).toString();

        if ( auto* model = m_config.model() )
        {
            if ( !preselectGroup.isEmpty() )
            {
                // Default Apps path — make sure exactly the named group
                // is checked. clearSelections() first to wipe whatever
                // the netinstall.yaml `selected:` defaults set, so a
                // user who bounces between choices doesn't end up with
                // cumulative selections from earlier visits.
                model->clearSelections();
                model->setSelections( QStringList { preselectGroup } );
            }
            else
            {
                // OS Only path — nothing should be installed from
                // netinstall, regardless of any `selected: true`
                // defaults in the YAML.
                model->clearSelections();
            }
        }
    }

    // Defer one event-loop turn so the activate() transition can finish
    // before navigation kicks off — avoids a brief one-frame paint of
    // the empty netinstall page.
    auto* vm = Calamares::ViewManager::instance();
    if ( backward )
    {
        QTimer::singleShot( 0, vm, &Calamares::ViewManager::back );
    }
    else
    {
        QTimer::singleShot( 0, vm, &Calamares::ViewManager::next );
    }
}

void
NetInstallViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config.setConfigurationMap( configurationMap );
}
