/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 MainstreamOS
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 */

#ifndef INSTALLMETHOD_VIEWSTEP_H
#define INSTALLMETHOD_VIEWSTEP_H

#include "DllMacro.h"

#include "utils/PluginFactory.h"
#include "viewpages/ViewStep.h"

#include <QObject>
#include <QVariantMap>

namespace InstallMethod
{
class Config;
class InstallMethodPage;
}  // namespace InstallMethod

class PLUGINDLLEXPORT InstallMethodViewStep : public Calamares::ViewStep
{
    Q_OBJECT

public:
    explicit InstallMethodViewStep( QObject* parent = nullptr );
    ~InstallMethodViewStep() override;

    QString prettyName() const override;
    QString prettyStatus() const override;

    QWidget* widget() override;

    bool isNextEnabled() const override;
    bool isBackEnabled() const override;

    bool isAtBeginning() const override;
    bool isAtEnd() const override;

    Calamares::JobList jobs() const override;

    void setConfigurationMap( const QVariantMap& configurationMap ) override;

private:
    InstallMethod::Config*            m_config;
    InstallMethod::InstallMethodPage* m_widget;
};

CALAMARES_PLUGIN_FACTORY_DECLARATION( InstallMethodViewStepFactory )

#endif
