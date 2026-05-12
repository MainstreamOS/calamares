/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2026 MainstreamOS
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 */

#ifndef INSTALLMETHOD_PAGE_H
#define INSTALLMETHOD_PAGE_H

#include "Config.h"

#include <QFrame>
#include <QLabel>
#include <QList>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

namespace InstallMethod
{

class Config;

/**
 * One clickable card on the install-method picker.
 *
 * Visually: an M3-style outlined surface that lights up with the brand
 * accent when selected. Composed by hand (no .ui file) because the
 * dynamic selected/unselected stylesheet swap is cleaner to express in
 * C++ than via Designer property bindings.
 */
class MethodCard : public QFrame
{
    Q_OBJECT
public:
    /** @param iconResourcePath  qrc path to a designed multi-color SVG
     *                           (e.g. ":/installmethod/icons/default-apps.svg").
     *                           The SVG already carries its Mainstream
     *                           palette so we render it as-is — selection
     *                           feedback lives on the card border /
     *                           background, not on the icon itself.
     *                           Mirrors how M3 cards treat illustrations
     *                           as static container content. */
    MethodCard( Choice choice,
                const QString& iconResourcePath,
                const QString& title,
                const QString& description,
                QWidget* parent = nullptr );

    Choice choice() const { return m_choice; }
    bool   isSelected() const { return m_selected; }

    /** Switch this card into / out of the selected visual state. */
    void setSelected( bool selected );

signals:
    void picked( InstallMethod::Choice );

protected:
    void mousePressEvent( QMouseEvent* event ) override;
    void keyPressEvent( QKeyEvent* event ) override;
    void enterEvent( QEnterEvent* event ) override;
    void leaveEvent( QEvent* event ) override;

private:
    void applyStyle();

    Choice  m_choice;
    bool    m_selected = false;
    bool    m_hovered  = false;
    QLabel* m_iconLabel = nullptr;    // hosts the rendered QPixmap
};

/**
 * The full installmethod page — a header (title + intro) above a
 * horizontal row of three MethodCards that share the available width
 * equally. Each card carries a large custom SVG glyph above its
 * title and description. Cards are mutually exclusive and write their
 * pick straight into the Config object passed in.
 */
class InstallMethodPage : public QWidget
{
    Q_OBJECT
public:
    explicit InstallMethodPage( Config* config, QWidget* parent = nullptr );

private slots:
    void onCardPicked( InstallMethod::Choice picked );
    void onConfigChanged( InstallMethod::Choice newChoice );

private:
    Config*             m_config;
    QList<MethodCard*>  m_cards;
};

}  // namespace InstallMethod

#endif
