/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 MainstreamOS
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 */

#ifndef INSTALLMETHOD_CONFIG_H
#define INSTALLMETHOD_CONFIG_H

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace InstallMethod
{

/**
 * Five install-method choices the user can pick on the "Get Started"
 * card page that runs immediately before netinstall:
 *
 *   Default    — auto-install the curated default-apps netinstall
 *                group and skip the netinstall page entirely
 *   Custom     — proceed to the netinstall page for hand-picking
 *   Developer  — auto-install the developer-tools netinstall group
 *                and skip the netinstall page
 *   Gaming     — auto-install the gaming netinstall group and skip
 *                the netinstall page
 *   OsOnly     — install nothing from netinstall and skip the page
 *
 * The choice is exposed both as a strong enum (for in-process consumers)
 * and as a string written to GlobalStorage so the netinstall ViewStep
 * can read it back without depending on this module's headers.
 */
enum class Choice
{
    Default,
    Custom,
    Developer,
    Gaming,
    OsOnly
};

QString choiceToString( Choice c );
Choice choiceFromString( const QString& s );

class Config : public QObject
{
    Q_OBJECT
    Q_PROPERTY( QString choice READ choiceString WRITE setChoiceString NOTIFY choiceChanged )

public:
    explicit Config( QObject* parent = nullptr );

    void setConfigurationMap( const QVariantMap& configurationMap );

    Choice  choice() const { return m_choice; }
    QString choiceString() const { return choiceToString( m_choice ); }

    /** The netinstall group name pre-checked for each pre-canned choice.
     *  All come from installmethod.conf keys (defaultAppsGroupName,
     *  developerGroupName, gamingGroupName). If a key is missing or
     *  empty the corresponding card still appears but writes an empty
     *  preselect group — netinstall then leaves package selections
     *  alone, which falls back to whatever `selected: true` defaults
     *  the YAML carries. */
    QString defaultAppsGroupName() const { return m_defaultAppsGroupName; }
    QString developerGroupName()   const { return m_developerGroupName; }
    QString gamingGroupName()      const { return m_gamingGroupName; }

    /** Helper that maps a Choice to the netinstall group it should
     *  pre-select. Custom and OsOnly both return an empty string —
     *  Custom because the user picks manually, OsOnly because nothing
     *  should be checked. */
    QString preselectGroupFor( Choice c ) const;

public slots:
    void setChoice( Choice c );
    void setChoiceString( const QString& s );

signals:
    void choiceChanged( Choice );

private:
    /** Push the current choice to GlobalStorage so netinstall and other
     *  downstream consumers can react. Three keys are written:
     *      installmethod_choice              — string: default|custom|os-only
     *      installmethod_skipnetinstall      — bool: true unless choice is custom
     *      installmethod_preselect_group     — string: group name to pre-check
     *                                          (empty when choice is os-only) */
    void writeToGlobalStorage() const;

    Choice  m_choice = Choice::Default;
    QString m_defaultAppsGroupName;
    QString m_developerGroupName;
    QString m_gamingGroupName;
};

}  // namespace InstallMethod

#endif
