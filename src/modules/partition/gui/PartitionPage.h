/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2018-2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2019 Collabora Ltd
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef PARTITIONPAGE_H
#define PARTITIONPAGE_H

#include <QMutex>
#include <QScopedPointer>
#include <QWidget>

class Config;
class PartitionCoreModule;
class Ui_PartitionPage;

class Device;
class Partition;

/**
 * The user interface for the module.
 *
 * Shows the information exposed by PartitionCoreModule and asks it to schedule
 * jobs according to user actions.
 */
class PartitionPage : public QWidget
{
    Q_OBJECT
public:
    explicit PartitionPage( PartitionCoreModule* core, const Config & config, QWidget* parent = nullptr );
    ~PartitionPage() override;

    void onRevertClicked();

    int selectedDeviceIndex();
    void selectDeviceByIndex( int index );

    /** @brief Does the layout meet what the configuration requires?
     *
     * Only the checks that *requireBootableLayout* and *requireFormattedRoot*
     * ask for are made, so without either this is always @c true. When it
     * is @c false, the page shows why.
     */
    bool isLayoutAcceptable() const { return m_layoutProblems.isEmpty(); }

Q_SIGNALS:
    void layoutAcceptableChanged( bool acceptable );

private Q_SLOTS:
    /// @brief Update everything when the base device changes
    void updateFromCurrentDevice();
    /** @brief Update when the selected device for boot loader changes
     *
     * With the selector hidden, the path follows the disk holding /boot
     * (or / when there is no separate /boot) after every change to the layout.
     */
    void updateBootLoaderInstallPath();
    /// @brief Explicitly selected boot loader path
    void updateSelectedBootLoaderIndex();
    /// @brief After boot loader model changes, try to preserve previously set value
    void restoreSelectedBootLoader();
    /// @brief Make the selections in each widget match
    void reconcileSelections();
    /// @brief Check the layout again and show what, if anything, is wrong with it
    void updateLayoutProblems();

private:
    QScopedPointer< Ui_PartitionPage > m_ui;
    PartitionCoreModule* m_core;
    void updateButtons();
    void onNewPartitionTableClicked();
    void onNewVolumeGroupClicked();
    void onResizeVolumeGroupClicked();
    void onDeactivateVolumeGroupClicked();
    void onRemoveVolumeGroupClicked();
    void onCreateClicked();
    void onEditClicked();
    void onDeleteClicked();
    void onPartitionViewActivated();
    void onPartitionModelReset();

    void updatePartitionToCreate( Device*, Partition* );
    void editExistingPartition( Device*, Partition* );
    void updateBootLoaderIndex();

    /**
     * @brief Check if a new partition can be created (as primary) on the device.
     *
     * Returns true if a new partition can be created on the device. Provides
     * a warning popup and returns false if it cannot.
     */
    bool checkCanCreate( Device* );

    QStringList getCurrentUsedMountpoints();

    /// @brief Everything in the layout that keeps it from being accepted, as sentences
    QStringList layoutProblems() const;

    QMutex m_revertMutex;
    int m_lastSelectedBootLoaderIndex;
    bool m_isEfi;
    bool m_requireBootableLayout;
    bool m_requireFormattedRoot;
    bool m_showBootLoaderSelector;
    QStringList m_layoutProblems;
};

#endif  // PARTITIONPAGE_H
