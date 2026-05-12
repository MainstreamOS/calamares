/*
 *   SPDX-FileCopyrightText: 2016 Luca Giambonini <almack@chakraos.org>
 *   SPDX-FileCopyrightText: 2016 Lisa Vitolo     <shainer@chakraos.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef NETINSTALLVIEWSTEP_H
#define NETINSTALLVIEWSTEP_H

#include "Config.h"

#include "DllMacro.h"
#include "utils/PluginFactory.h"
#include "viewpages/ViewStep.h"

#include <QVariant>

class NetInstallPage;

class PLUGINDLLEXPORT NetInstallViewStep : public Calamares::ViewStep
{
    Q_OBJECT

public:
    explicit NetInstallViewStep( QObject* parent = nullptr );
    ~NetInstallViewStep() override;

    QString prettyName() const override;

    QWidget* widget() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;

    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    Calamares::JobList jobs() const override;

    void onActivate() override;

    // Leaving the page; store all selected packages for later installation.
    void onLeave() override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

public slots:
    void nextIsReady();

private:
    /** True when the upstream installmethod module asked us to skip
     *  the netinstall page on this entry. Set by reading GlobalStorage
     *  in onActivate(). */
    bool installmethodWantsSkip() const;

    /** Apply the pre-selection requested by installmethod (or clear all
     *  selections for the OS Only path) and schedule a one-tick
     *  ViewManager::next()/back() to bounce past this page in the
     *  given direction. Forward auto-skip lands on Summary; backward
     *  auto-skip lands on installmethod, so a Back from Summary can
     *  walk the user straight back to the picker without an empty
     *  netinstall page in between. */
    void applyInstallMethodChoiceAndAdvance( bool backward );

    Config m_config;

    NetInstallPage* m_widget;
    bool m_nextEnabled = false;

    /** Set in onActivate() when installmethod wants us to skip but the
     *  YAML group data hasn't finished loading yet. nextIsReady() picks
     *  this up and finishes the skip once loading completes. */
    bool m_pendingSkip = false;

    /** Captured along with m_pendingSkip — the direction we should
     *  bounce in once nextIsReady() fires. */
    bool m_pendingSkipBackward = false;

    /** Last step index ViewManager announced as current, tracked via
     *  its currentStepChanged signal. The signal fires AFTER each new
     *  step's onActivate has run, so during our own onActivate this
     *  value still reflects the step the user was on before they
     *  navigated here — that's what lets us tell forward from back. */
    int m_lastSeenStepIndex = -1;
};

CALAMARES_PLUGIN_FACTORY_DECLARATION( NetInstallViewStepFactory )

#endif  // NETINSTALLVIEWSTEP_H
