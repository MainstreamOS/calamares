/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2015-2016 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2018-2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-FileCopyrightText: 2018 Andrius Štikonas <andrius@stikonas.eu>
 *   SPDX-FileCopyrightText: 2018 Caio Jordão Carvalho <caiojcarvalho@gmail.com>
 *   SPDX-FileCopyrightText: 2019 Collabora Ltd
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "PartitionPage.h"

// Local
#include "Config.h"
#include "core/BootLoaderModel.h"
#include "core/DeviceModel.h"
#include "core/KPMHelpers.h"
#include "core/PartUtils.h"
#include "core/PartitionCoreModule.h"
#include "core/PartitionInfo.h"
#include "core/PartitionModel.h"
#include "gui/CreatePartitionDialog.h"
#include "gui/CreateVolumeGroupDialog.h"
#include "gui/EditExistingPartitionDialog.h"
#include "gui/ResizeVolumeGroupDialog.h"
#include "gui/ScanningDialog.h"

#include "ui_CreatePartitionTableDialog.h"
#include "ui_PartitionPage.h"

#include "Branding.h"
#include "GlobalStorage.h"
#include "JobQueue.h"
#include "partition/PartitionIterator.h"
#include "partition/PartitionQuery.h"
#include "utils/Gui.h"
#include "utils/Logger.h"
#include "utils/Retranslator.h"
#include "utils/Units.h"
#include "widgets/TranslationFix.h"

#include <kpmcore/core/device.h>
#include <kpmcore/core/partition.h>
#include <kpmcore/core/softwareraid.h>
#include <kpmcore/ops/deactivatevolumegroupoperation.h>
#include <kpmcore/ops/removevolumegroupoperation.h>

#include <QDir>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QPointer>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <mutex>

PartitionPage::PartitionPage( PartitionCoreModule* core, const Config & config, QWidget* parent )
    : QWidget( parent )
    , m_ui( new Ui_PartitionPage )
    , m_core( core )
    , m_lastSelectedBootLoaderIndex( -1 )
    , m_isEfi( PartUtils::isEfiSystem() )
    , m_requireBootableLayout( config.requireBootableLayout() )
    , m_requireFormattedRoot( config.requireFormattedRoot() )
    , m_showBootLoaderSelector( config.showBootLoaderSelector() )
{
    if ( config.installChoice() != Config::InstallChoice::Manual )
    {
        cWarning() << "Manual partitioning page created without user choosing manual-partitioning.";
    }

    m_ui->setupUi( this );
    m_ui->partitionLabelsView->setVisible(
        Calamares::JobQueue::instance()->globalStorage()->value( "alwaysShowPartitionLabels" ).toBool() );
    m_ui->deviceComboBox->setModel( m_core->deviceModel() );
    m_ui->bootLoaderComboBox->setModel( m_core->bootLoaderModel() );
    connect(
        m_core->bootLoaderModel(), &QAbstractItemModel::modelReset, this, &PartitionPage::restoreSelectedBootLoader );
    // The boot loader model is rebuilt after every change to the layout,
    // so its reset is the one notice that covers them all.
    connect( m_core->bootLoaderModel(), &QAbstractItemModel::modelReset, this, &PartitionPage::updateLayoutProblems );
    PartitionBarsView::NestedPartitionsMode mode
        = Calamares::JobQueue::instance()->globalStorage()->value( "drawNestedPartitions" ).toBool()
        ? PartitionBarsView::DrawNestedPartitions
        : PartitionBarsView::NoNestedPartitions;
    m_ui->partitionBarsView->setNestedPartitionsMode( mode );
    m_ui->lvmButtonPanel->setVisible( config.isLVMEnabled() );

    updateButtons();
    updateBootLoaderInstallPath();

    updateFromCurrentDevice();

    connect( m_ui->deviceComboBox, &QComboBox::currentTextChanged, this, &PartitionPage::updateFromCurrentDevice );
    connect( m_ui->bootLoaderComboBox,
             QOverload< int >::of( &QComboBox::activated ),
             this,
             &PartitionPage::updateSelectedBootLoaderIndex );
    connect(
        m_ui->bootLoaderComboBox, &QComboBox::currentTextChanged, this, &PartitionPage::updateBootLoaderInstallPath );

    connect( m_core, &PartitionCoreModule::isDirtyChanged, m_ui->revertButton, &QWidget::setEnabled );

    connect(
        m_ui->partitionTreeView, &QAbstractItemView::doubleClicked, this, &PartitionPage::onPartitionViewActivated );
    connect( m_ui->revertButton, &QAbstractButton::clicked, this, &PartitionPage::onRevertClicked );
    connect( m_ui->newVolumeGroupButton, &QAbstractButton::clicked, this, &PartitionPage::onNewVolumeGroupClicked );
    connect(
        m_ui->resizeVolumeGroupButton, &QAbstractButton::clicked, this, &PartitionPage::onResizeVolumeGroupClicked );
    connect( m_ui->deactivateVolumeGroupButton,
             &QAbstractButton::clicked,
             this,
             &PartitionPage::onDeactivateVolumeGroupClicked );
    connect(
        m_ui->removeVolumeGroupButton, &QAbstractButton::clicked, this, &PartitionPage::onRemoveVolumeGroupClicked );
    connect(
        m_ui->newPartitionTableButton, &QAbstractButton::clicked, this, &PartitionPage::onNewPartitionTableClicked );
    connect( m_ui->createButton, &QAbstractButton::clicked, this, &PartitionPage::onCreateClicked );
    connect( m_ui->editButton, &QAbstractButton::clicked, this, &PartitionPage::onEditClicked );
    connect( m_ui->deleteButton, &QAbstractButton::clicked, this, &PartitionPage::onDeleteClicked );

    // A distribution whose boot loader step picks the disk on its own would
    // otherwise offer a choice that is then ignored.
    if ( m_isEfi || !m_showBootLoaderSelector )
    {
        m_ui->bootLoaderComboBox->hide();
        m_ui->label_3->hide();
    }

    const int iconSize = Calamares::defaultFontHeight();
    m_ui->layoutProblemIcon->setPixmap(
        Calamares::defaultPixmap( Calamares::StatusWarning, Calamares::Original, QSize( iconSize, iconSize ) ) );
    m_ui->layoutProblemPanel->hide();

    CALAMARES_RETRANSLATE(
        m_ui->retranslateUi( this );
        m_core->bootLoaderModel()->update(); // Need to re-translate entries in the combo-box
    );
}

PartitionPage::~PartitionPage() {}

void
PartitionPage::updateButtons()
{
    bool allow_create = false, allow_create_table = false, allow_edit = false, allow_delete = false;
    bool currentDeviceIsVG = false, isDeactivable = false;
    bool isRemovable = false, isVGdeactivated = false;

    QModelIndex index = m_ui->partitionTreeView->currentIndex();
    if ( index.isValid() )
    {
        const PartitionModel* model = static_cast< const PartitionModel* >( index.model() );
        Q_ASSERT( model );
        Partition* partition = model->partitionForIndex( index );
        Q_ASSERT( partition );
        const bool isFree = Calamares::Partition::isPartitionFreeSpace( partition );
        const bool isExtended = partition->roles().has( PartitionRole::Extended );

        // An extended partition can have a "free space" child; that one does
        // not count as a real child. If there are more children, at least one
        // is a real one and we should not allow the extended partition to be
        // deleted.
        const bool hasChildren = isExtended
            && ( partition->children().length() > 1
                 || ( partition->children().length() == 1
                      && !Calamares::Partition::isPartitionFreeSpace( partition->children().at( 0 ) ) ) );

        const bool isInVG = m_core->isInVG( partition );

        allow_create = isFree;

        // Keep it simple for now: do not support editing extended partitions as
        // it does not work with our current edit implementation which is
        // actually remove + add. This would not work with extended partitions
        // because they need to be created *before* creating logical partitions
        // inside them, so an edit must be applied without altering the job
        // order.
        // TODO: See if LVM PVs can be edited in Calamares
        allow_edit = !isFree && !isExtended;
        allow_delete = !isFree && !isInVG && !hasChildren;
    }

    if ( m_ui->deviceComboBox->currentIndex() >= 0 )
    {
        Device* device = nullptr;
        QModelIndex deviceIndex = m_core->deviceModel()->index( m_ui->deviceComboBox->currentIndex(), 0 );
        if ( deviceIndex.isValid() )
        {
            device = m_core->deviceModel()->deviceForIndex( deviceIndex );
        }
        if ( !device )
        {
            cWarning() << "Device for updateButtons is nullptr";
        }
        else if ( device->type() != Device::Type::LVM_Device )
        {
            allow_create_table = true;

            if ( device->type() == Device::Type::SoftwareRAID_Device
                 && static_cast< SoftwareRAID* >( device )->status() == SoftwareRAID::Status::Inactive )
            {
                allow_create_table = false;
                allow_create = false;
            }
        }
        else
        {
            currentDeviceIsVG = true;

            LvmDevice* lvmDevice = dynamic_cast< LvmDevice* >( m_core->deviceModel()->deviceForIndex( deviceIndex ) );

            isDeactivable = DeactivateVolumeGroupOperation::isDeactivatable( lvmDevice );
            isRemovable = RemoveVolumeGroupOperation::isRemovable( lvmDevice );

            isVGdeactivated = m_core->isVGdeactivated( lvmDevice );

            if ( isVGdeactivated )
            {
                m_ui->revertButton->setEnabled( true );
            }
        }
    }

    m_ui->createButton->setEnabled( allow_create );
    m_ui->editButton->setEnabled( allow_edit );
    m_ui->deleteButton->setEnabled( allow_delete );
    m_ui->newPartitionTableButton->setEnabled( allow_create_table );
    m_ui->resizeVolumeGroupButton->setEnabled( currentDeviceIsVG && !isVGdeactivated );
    m_ui->deactivateVolumeGroupButton->setEnabled( currentDeviceIsVG && isDeactivable && !isVGdeactivated );
    m_ui->removeVolumeGroupButton->setEnabled( currentDeviceIsVG && isRemovable );
}

void
PartitionPage::onNewPartitionTableClicked()
{
    QModelIndex index = m_core->deviceModel()->index( m_ui->deviceComboBox->currentIndex(), 0 );
    Q_ASSERT( index.isValid() );
    Device* device = m_core->deviceModel()->deviceForIndex( index );

    QPointer< QDialog > dlg = new QDialog( this );
    Ui_CreatePartitionTableDialog ui;
    ui.setupUi( dlg.data() );
    QString areYouSure = tr( "Are you sure you want to create a new partition table on %1?" ).arg( device->name() );
    if ( PartUtils::isEfiSystem() )
    {
        ui.gptRadioButton->setChecked( true );
    }
    else
    {
        ui.mbrRadioButton->setChecked( true );
    }

    ui.areYouSureLabel->setText( areYouSure );
    if ( dlg->exec() == QDialog::Accepted )
    {
        PartitionTable::TableType type = ui.mbrRadioButton->isChecked() ? PartitionTable::msdos : PartitionTable::gpt;
        m_core->createPartitionTable( device, type );
    }
    delete dlg;
    // PartionModelReset isn't emitted after createPartitionTable, so we have to manually update
    // the bootLoader index after the reset.
    updateBootLoaderIndex();
}

bool
PartitionPage::checkCanCreate( Device* device )
{
    auto table = device->partitionTable();

    if ( KPMHelpers::isMSDOSPartition( table->type() ) )
    {
        cDebug() << "Checking MSDOS partition" << table->numPrimaries() << "primaries, max" << table->maxPrimaries();

        if ( ( table->numPrimaries() >= table->maxPrimaries() ) && !table->hasExtended() )
        {
            QMessageBox mb(
                QMessageBox::Warning,
                tr( "Can not create new partition" ),
                tr( "The partition table on %1 already has %2 primary partitions, and no more can be added. "
                    "Please remove one primary partition and add an extended partition, instead." )
                    .arg( device->name() )
                    .arg( table->numPrimaries() ),
                QMessageBox::Ok );
            Calamares::fixButtonLabels( &mb );
            mb.exec();
            return false;
        }
        return true;
    }
    else
    {
        return true;  // GPT is fine
    }
}

void
PartitionPage::onNewVolumeGroupClicked()
{
    QString vgName;
    QVector< const Partition* > selectedPVs;
    qint64 peSize = 4;

    QVector< const Partition* > availablePVs;

    for ( const Partition* p : m_core->lvmPVs() )
    {
        if ( !m_core->isInVG( p ) )
        {
            availablePVs << p;
        }
    }

    QPointer< CreateVolumeGroupDialog > dlg
        = new CreateVolumeGroupDialog( vgName, selectedPVs, availablePVs, peSize, this );

    if ( dlg->exec() == QDialog::Accepted )
    {
        QModelIndex partitionIndex = m_ui->partitionTreeView->currentIndex();

        if ( partitionIndex.isValid() )
        {
            const PartitionModel* model = static_cast< const PartitionModel* >( partitionIndex.model() );
            Q_ASSERT( model );
            Partition* partition = model->partitionForIndex( partitionIndex );
            Q_ASSERT( partition );

            // Disable delete button if current partition was selected to be in VG
            // TODO: Should Calamares edit LVM PVs which are in VGs?
            if ( selectedPVs.contains( partition ) )
            {
                m_ui->deleteButton->setEnabled( false );
            }
        }

        QModelIndex deviceIndex = m_core->deviceModel()->index( m_ui->deviceComboBox->currentIndex(), 0 );
        Q_ASSERT( deviceIndex.isValid() );

        QVariant previousIndexDeviceData = m_core->deviceModel()->data( deviceIndex, Qt::ToolTipRole );

        // Creating new VG
        m_core->createVolumeGroup( vgName, selectedPVs, peSize );

        // As createVolumeGroup method call resets deviceModel,
        // is needed to set the current index in deviceComboBox as the previous one
        int previousIndex = m_ui->deviceComboBox->findData( previousIndexDeviceData, Qt::ToolTipRole );

        m_ui->deviceComboBox->setCurrentIndex( ( previousIndex < 0 ) ? 0 : previousIndex );
        updateFromCurrentDevice();
    }

    delete dlg;
}

void
PartitionPage::onResizeVolumeGroupClicked()
{
    QModelIndex deviceIndex = m_core->deviceModel()->index( m_ui->deviceComboBox->currentIndex(), 0 );
    LvmDevice* device = dynamic_cast< LvmDevice* >( m_core->deviceModel()->deviceForIndex( deviceIndex ) );

    Q_ASSERT( device && device->type() == Device::Type::LVM_Device );

    QVector< const Partition* > availablePVs;
    QVector< const Partition* > selectedPVs;

    for ( const Partition* p : m_core->lvmPVs() )
    {
        if ( !m_core->isInVG( p ) )
        {
            availablePVs << p;
        }
    }

    QPointer< ResizeVolumeGroupDialog > dlg = new ResizeVolumeGroupDialog( device, availablePVs, selectedPVs, this );

    if ( dlg->exec() == QDialog::Accepted )
    {
        m_core->resizeVolumeGroup( device, selectedPVs );
    }

    delete dlg;
}

void
PartitionPage::onDeactivateVolumeGroupClicked()
{
    QModelIndex deviceIndex = m_core->deviceModel()->index( m_ui->deviceComboBox->currentIndex(), 0 );
    LvmDevice* device = dynamic_cast< LvmDevice* >( m_core->deviceModel()->deviceForIndex( deviceIndex ) );

    Q_ASSERT( device && device->type() == Device::Type::LVM_Device );

    m_core->deactivateVolumeGroup( device );

    updateFromCurrentDevice();

    PartitionModel* model = m_core->partitionModelForDevice( device );
    model->update();
}

void
PartitionPage::onRemoveVolumeGroupClicked()
{
    QModelIndex deviceIndex = m_core->deviceModel()->index( m_ui->deviceComboBox->currentIndex(), 0 );
    LvmDevice* device = dynamic_cast< LvmDevice* >( m_core->deviceModel()->deviceForIndex( deviceIndex ) );

    Q_ASSERT( device && device->type() == Device::Type::LVM_Device );

    m_core->removeVolumeGroup( device );
}

void
PartitionPage::onCreateClicked()
{
    QModelIndex index = m_ui->partitionTreeView->currentIndex();
    Q_ASSERT( index.isValid() );

    const PartitionModel* model = static_cast< const PartitionModel* >( index.model() );
    Partition* partition = model->partitionForIndex( index );
    Q_ASSERT( partition );

    if ( !checkCanCreate( model->device() ) )
    {
        return;
    }

    QPointer< CreatePartitionDialog > dlg = new CreatePartitionDialog(
        m_core, model->device(), CreatePartitionDialog::FreeSpace { partition }, getCurrentUsedMountpoints(), this );
    if ( dlg->exec() == QDialog::Accepted )
    {
        Partition* newPart = dlg->getNewlyCreatedPartition();
        m_core->createPartition( model->device(), newPart, dlg->newFlags() );
    }
    delete dlg;
}

void
PartitionPage::onEditClicked()
{
    QModelIndex index = m_ui->partitionTreeView->currentIndex();
    Q_ASSERT( index.isValid() );

    const PartitionModel* model = static_cast< const PartitionModel* >( index.model() );
    Partition* partition = model->partitionForIndex( index );
    Q_ASSERT( partition );

    if ( Calamares::Partition::isPartitionNew( partition ) )
    {
        updatePartitionToCreate( model->device(), partition );
    }
    else
    {
        editExistingPartition( model->device(), partition );
    }
}

void
PartitionPage::onDeleteClicked()
{
    QModelIndex index = m_ui->partitionTreeView->currentIndex();
    Q_ASSERT( index.isValid() );

    const PartitionModel* model = static_cast< const PartitionModel* >( index.model() );
    Partition* partition = model->partitionForIndex( index );
    Q_ASSERT( partition );

    m_core->deletePartition( model->device(), partition );
}


void
PartitionPage::onRevertClicked()
{
    ScanningDialog::run(
        QtConcurrent::run(
            [ this ]
            {
                QMutexLocker locker( &m_revertMutex );

                int oldIndex = m_ui->deviceComboBox->currentIndex();
                m_core->revertAllDevices();
                m_ui->deviceComboBox->setCurrentIndex( ( oldIndex < 0 ) ? 0 : oldIndex );
                updateFromCurrentDevice();
            } ),
        [ this ]
        {
            m_lastSelectedBootLoaderIndex = -1;
            if ( m_ui->bootLoaderComboBox->currentIndex() < 0 )
            {
                m_ui->bootLoaderComboBox->setCurrentIndex( 0 );
            }
            // Only the hidden selector skips its updates while the revert
            // runs; a shown one has already set the path from the pick.
            if ( !m_showBootLoaderSelector )
            {
                updateBootLoaderInstallPath();
            }
            updateLayoutProblems();
        },
        this );
}

void
PartitionPage::onPartitionViewActivated()
{
    QModelIndex index = m_ui->partitionTreeView->currentIndex();
    if ( !index.isValid() )
    {
        return;
    }

    const PartitionModel* model = static_cast< const PartitionModel* >( index.model() );
    Q_ASSERT( model );
    Partition* partition = model->partitionForIndex( index );
    Q_ASSERT( partition );

    // Use the buttons to trigger the actions so that they do nothing if they
    // are disabled. Alternatively, the code could use QAction to centralize,
    // but I don't expect there will be other occurences of triggering the same
    // action from multiple UI elements in this page, so it does not feel worth
    // the price.
    if ( Calamares::Partition::isPartitionFreeSpace( partition ) )
    {
        m_ui->createButton->click();
    }
    else
    {
        m_ui->editButton->click();
    }
}

void
PartitionPage::updatePartitionToCreate( Device* device, Partition* partition )
{
    QStringList mountPoints = getCurrentUsedMountpoints();
    mountPoints.removeOne( PartitionInfo::mountPoint( partition ) );

    QPointer< CreatePartitionDialog > dlg
        = new CreatePartitionDialog( m_core, device, CreatePartitionDialog::FreshPartition { partition }, mountPoints, this );
    if ( dlg->exec() == QDialog::Accepted )
    {
        Partition* newPartition = dlg->getNewlyCreatedPartition();
        m_core->deletePartition( device, partition );
        m_core->createPartition( device, newPartition, dlg->newFlags() );
    }
    delete dlg;
}

void
PartitionPage::editExistingPartition( Device* device, Partition* partition )
{
    QStringList mountPoints = getCurrentUsedMountpoints();
    mountPoints.removeOne( PartitionInfo::mountPoint( partition ) );

    QPointer< EditExistingPartitionDialog > dlg
        = new EditExistingPartitionDialog( m_core, device, partition, mountPoints, m_requireFormattedRoot, this );
    if ( dlg->exec() == QDialog::Accepted )
    {
        dlg->applyChanges( m_core );
    }
    delete dlg;

    updateBootLoaderInstallPath();
    // applyChanges() sets the partition's format mark and flags after the
    // last model refresh it causes, so the check that refresh ran can be stale.
    updateLayoutProblems();
}

/// @brief The partition set to be mounted at @p mountPoint, and the device it is on
static std::pair< Partition*, Device* >
findMountedPartition( const PartitionCoreModule* core, const QString& mountPoint )
{
    if ( !mountPoint.isEmpty() )
    {
        const DeviceModel* devices = core->deviceModel();
        for ( int row = 0; row < devices->rowCount(); ++row )
        {
            Device* device = devices->deviceForIndex( devices->index( row ) );
            for ( auto it = Calamares::Partition::PartitionIterator::begin( device );
                  it != Calamares::Partition::PartitionIterator::end( device );
                  ++it )
            {
                if ( PartitionInfo::mountPoint( *it ) == mountPoint )
                {
                    return { *it, device };
                }
            }
        }
    }
    return { nullptr, nullptr };
}

/** @brief The disk holding /boot, or / when there is no separate /boot
 *
 * Empty when that partition is not on a disk the boot loader model offers,
 * such as a logical volume or an array, which cannot take a boot loader.
 */
static QString
diskHoldingBoot( const PartitionCoreModule* core )
{
    for ( const QString& mountPoint : { QStringLiteral( "/boot" ), QStringLiteral( "/" ) } )
    {
        const auto [ partition, device ] = findMountedPartition( core, mountPoint );
        if ( partition )
        {
            const bool offered = core->bootLoaderModel()->findBootLoader( device->deviceNode() ).second != nullptr;
            return offered ? device->deviceNode() : QString();
        }
    }
    return QString();
}

void
PartitionPage::updateBootLoaderInstallPath()
{
    if ( m_isEfi )
    {
        return;
    }

    // Without the selector nobody picks an entry, and the first one names the
    // first disk. A boot loader step that picks the disk on its own writes to
    // the one holding /boot, the disk the layout checks ask a BIOS boot
    // partition of, so the summary names that disk instead.
    if ( !m_showBootLoaderSelector )
    {
        // A revert frees the devices this walks on another thread, and its
        // callback works the path out again once it is done.
        std::unique_lock< QMutex > revertLock( m_revertMutex, std::try_to_lock );
        if ( !revertLock.owns_lock() )
        {
            return;
        }
        const QString disk = diskHoldingBoot( m_core );
        revertLock.unlock();
        if ( !disk.isEmpty() )
        {
            cDebug() << "PartitionPage::updateBootLoaderInstallPath" << disk << "holds /boot or /";
            m_core->setBootLoaderInstallPath( disk );
            return;
        }
    }

    // The hidden combo box only echoes the last path restored into it, which
    // after a revert can name a disk that no longer holds anything.
    const int row = m_showBootLoaderSelector ? m_ui->bootLoaderComboBox->currentIndex() : 0;
    QVariant var = m_ui->bootLoaderComboBox->itemData( row, BootLoaderModel::BootLoaderPathRole );
    if ( !var.isValid() )
    {
        return;
    }
    cDebug() << "PartitionPage::updateBootLoaderInstallPath" << var.toString();
    m_core->setBootLoaderInstallPath( var.toString() );
}

void
PartitionPage::updateSelectedBootLoaderIndex()
{
    m_lastSelectedBootLoaderIndex = m_ui->bootLoaderComboBox->currentIndex();
    cDebug() << "Selected bootloader index" << m_lastSelectedBootLoaderIndex;
}

void
PartitionPage::restoreSelectedBootLoader()
{
    // The model resets after every change to the layout, and a change can
    // move /boot or / to another disk while the hidden entry stays put.
    if ( !m_showBootLoaderSelector )
    {
        updateBootLoaderInstallPath();
    }
    Calamares::restoreSelectedBootLoader( *( m_ui->bootLoaderComboBox ), m_core->bootLoaderInstallPath() );
}

void
PartitionPage::reconcileSelections()
{
    QModelIndex selectedIndex = m_ui->partitionBarsView->selectionModel()->currentIndex();
    selectedIndex = selectedIndex.sibling( selectedIndex.row(), 0 );
    m_ui->partitionBarsView->setCurrentIndex( selectedIndex );
    m_ui->partitionLabelsView->setCurrentIndex( selectedIndex );
}

void
PartitionPage::updateFromCurrentDevice()
{
    QModelIndex index = m_core->deviceModel()->index( m_ui->deviceComboBox->currentIndex(), 0 );
    if ( !index.isValid() )
    {
        return;
    }

    Device* device = m_core->deviceModel()->deviceForIndex( index );

    QAbstractItemModel* oldModel = m_ui->partitionTreeView->model();
    if ( oldModel )
    {
        disconnect( oldModel, nullptr, this, nullptr );
    }

    PartitionModel* model = m_core->partitionModelForDevice( device );
    m_ui->partitionBarsView->setModel( model );
    m_ui->partitionLabelsView->setModel( model );
    m_ui->partitionTreeView->setModel( model );
    m_ui->partitionTreeView->expandAll();

    // Make all views use the same selection model.
    if ( m_ui->partitionBarsView->selectionModel() != m_ui->partitionTreeView->selectionModel()
         || m_ui->partitionBarsView->selectionModel() != m_ui->partitionLabelsView->selectionModel() )
    {
        // Tree view
        QItemSelectionModel* selectionModel = m_ui->partitionTreeView->selectionModel();
        m_ui->partitionTreeView->setSelectionModel( m_ui->partitionBarsView->selectionModel() );
        selectionModel->deleteLater();

        // Labels view
        selectionModel = m_ui->partitionLabelsView->selectionModel();
        m_ui->partitionLabelsView->setSelectionModel( m_ui->partitionBarsView->selectionModel() );
        selectionModel->deleteLater();
    }

    // This is necessary because even with the same selection model it might happen that
    // a !=0 column is selected in the tree view, which for some reason doesn't trigger a
    // timely repaint in the bars view.
    connect( m_ui->partitionBarsView->selectionModel(),
             &QItemSelectionModel::currentChanged,
             this,
             &PartitionPage::reconcileSelections,
             Qt::UniqueConnection );

    // Must be done here because we need to have a model set to define
    // individual column resize mode
    QHeaderView* header = m_ui->partitionTreeView->header();
    header->setSectionResizeMode( QHeaderView::ResizeToContents );
    header->setSectionResizeMode( 0, QHeaderView::Stretch );

    updateButtons();
    // Establish connection here because selection model is destroyed when
    // model changes
    connect( m_ui->partitionTreeView->selectionModel(),
             &QItemSelectionModel::currentChanged,
             [ this ]( const QModelIndex&, const QModelIndex& ) { updateButtons(); } );
    connect( model, &QAbstractItemModel::modelReset, this, &PartitionPage::onPartitionModelReset );
}

void
PartitionPage::onPartitionModelReset()
{
    m_ui->partitionTreeView->expandAll();
    updateButtons();
    updateBootLoaderIndex();
}

void
PartitionPage::updateBootLoaderIndex()
{
    // set bootloader back to user selected index
    if ( m_lastSelectedBootLoaderIndex >= 0 && m_ui->bootLoaderComboBox->count() )
    {
        m_ui->bootLoaderComboBox->setCurrentIndex( m_lastSelectedBootLoaderIndex );
    }
}

QStringList
PartitionPage::getCurrentUsedMountpoints()
{
    QModelIndex index = m_core->deviceModel()->index( m_ui->deviceComboBox->currentIndex(), 0 );
    if ( !index.isValid() )
    {
        return QStringList();
    }

    Device* device = m_core->deviceModel()->deviceForIndex( index );
    QStringList mountPoints;

    for ( Partition* partition : device->partitionTable()->children() )
    {
        const QString& mountPoint = PartitionInfo::mountPoint( partition );
        if ( !mountPoint.isEmpty() )
        {
            mountPoints << mountPoint;
        }
    }

    return mountPoints;
}

int
PartitionPage::selectedDeviceIndex()
{
    return m_ui->deviceComboBox->currentIndex();
}

void
PartitionPage::selectDeviceByIndex( int index )
{
    m_ui->deviceComboBox->setCurrentIndex( index );
}

// FAT12 is left out because the mount and fstab steps only know fat16 and
// fat32 as vfat.
static bool
isMountableFat( const Partition* partition )
{
    const auto type = partition->fileSystem().type();
    return type == FileSystem::Fat16 || type == FileSystem::Fat32;
}

static bool
isEncrypted( const Partition* partition )
{
    const auto type = partition->fileSystem().type();
    return type == FileSystem::Luks || type == FileSystem::Luks2;
}

// On BIOS the kernels and initramfs images live on a FAT /boot of their own,
// and this leaves room for more than one kernel.
static constexpr int biosBootMinimumMiB = 512;

QStringList
PartitionPage::layoutProblems() const
{
    QStringList problems;
    const auto [ root, rootDevice ] = findMountedPartition( m_core, QStringLiteral( "/" ) );

    if ( m_requireBootableLayout )
    {
        if ( m_isEfi )
        {
            const QString espMountPoint
                = Calamares::JobQueue::instance()->globalStorage()->value( "efiSystemPartition" ).toString();
            const QString espFlag = PartitionTable::flagName( KPM_PARTITION_FLAG_ESP );
            const auto [ esp, espDevice ] = findMountedPartition( m_core, espMountPoint );
            if ( !esp )
            {
                problems.append( tr( "Add an EFI system partition: a FAT32 (or FAT16) partition mounted at "
                                     "<strong>%1</strong> with the <strong>%2</strong> flag set.",
                                     "@info" )
                                     .arg( espMountPoint, espFlag ) );
            }
            else
            {
                if ( isEncrypted( esp ) )
                {
                    problems.append( tr( "The EFI system partition at <strong>%1</strong> cannot be encrypted, "
                                         "because the firmware reads it before anything is unlocked.",
                                         "@info" )
                                         .arg( espMountPoint ) );
                }
                else if ( !isMountableFat( esp ) )
                {
                    problems.append( tr( "The EFI system partition at <strong>%1</strong> has to be formatted as "
                                         "FAT32 (or FAT16).",
                                         "@info" )
                                         .arg( espMountPoint ) );
                }
                // On an MBR disk the flag only toggles the active bit, and
                // nothing here can give a partition the EFI system type.
                const PartitionTable* espTable = espDevice->partitionTable();
                if ( !espTable || espTable->type() != PartitionTable::TableType::gpt )
                {
                    problems.append( tr( "The EFI system partition at <strong>%1</strong> is on <strong>%2</strong>, "
                                         "which does not have a GPT partition table. Put the EFI system partition "
                                         "on a GPT disk, where it can be marked so the firmware finds it.",
                                         "@info" )
                                         .arg( espMountPoint, espDevice->deviceNode() ) );
                }
                else if ( !PartUtils::isEfiBootable( esp ) )
                {
                    problems.append(
                        tr( "The EFI system partition at <strong>%1</strong> needs the <strong>%2</strong> flag set.",
                            "@info" )
                            .arg( espMountPoint, espFlag ) );
                }
            }
        }
        else
        {
            const auto [ boot, bootDevice ] = findMountedPartition( m_core, QStringLiteral( "/boot" ) );
            if ( !boot )
            {
                problems.append( tr( "Add a separate partition mounted at <strong>/boot</strong>: FAT32 (or FAT16), "
                                     "not encrypted, and at least %1 MiB. On this computer the boot loader starts "
                                     "the system from there.",
                                     "@info" )
                                     .arg( biosBootMinimumMiB ) );
            }
            else
            {
                if ( isEncrypted( boot ) )
                {
                    problems.append( tr( "The partition mounted at <strong>/boot</strong> cannot be encrypted, "
                                         "because the boot loader reads it before anything is unlocked.",
                                         "@info" ) );
                }
                else if ( !isMountableFat( boot ) )
                {
                    problems.append( tr( "The partition mounted at <strong>/boot</strong> has to be formatted as "
                                         "FAT32 (or FAT16).",
                                         "@info" ) );
                }
                if ( Calamares::BytesToMiB( boot->capacity() ) < biosBootMinimumMiB )
                {
                    problems.append(
                        tr( "The partition mounted at <strong>/boot</strong> has to be at least %1 MiB.", "@info" )
                            .arg( biosBootMinimumMiB ) );
                }
                const PartitionTable* table = bootDevice->partitionTable();
                if ( bootDevice->type() == Device::Type::LVM_Device )
                {
                    problems.append( tr( "The partition mounted at <strong>/boot</strong> cannot be an LVM logical "
                                         "volume. Use a regular partition.",
                                         "@info" ) );
                }
                else if ( bootDevice->type() == Device::Type::SoftwareRAID_Device )
                {
                    problems.append( tr( "The partition mounted at <strong>/boot</strong> cannot be on software RAID. "
                                         "Use a regular partition.",
                                         "@info" ) );
                }
                else if ( table && table->type() == PartitionTable::TableType::gpt )
                {
                    const QString biosFlag = PartitionTable::flagName( KPM_PARTITION_FLAG( BiosGrub ) );
                    const auto biosBoot = PartUtils::biosBootPartitions( bootDevice );
                    if ( std::none_of( biosBoot.cbegin(), biosBoot.cend(), PartUtils::holdsNothing ) )
                    {
                        problems.append( tr( "The disk <strong>%1</strong> holding <strong>/boot</strong> has a GPT "
                                             "partition table, so the boot loader also needs a BIOS boot partition "
                                             "there. Create a small unformatted partition (at least 1 MiB) on it "
                                             "with the <strong>%2</strong> flag set.",
                                             "@info" )
                                             .arg( bootDevice->deviceNode(), biosFlag ) );
                    }
                    if ( const Partition* overwritten = PartUtils::overwrittenByBootLoader( biosBoot ) )
                    {
                        QString name = Calamares::Partition::isPartitionNew( overwritten )
                            ? tr( "New Partition", "@title" )
                            : overwritten->partitionPath();
                        const QString mountPoint = PartitionInfo::mountPoint( overwritten );
                        if ( !mountPoint.isEmpty() )
                        {
                            name += QStringLiteral( " (%1)" ).arg( mountPoint );
                        }
                        problems.append( tr( "The boot loader writes itself into the first partition with the "
                                             "<strong>%2</strong> flag on <strong>%1</strong>, and would overwrite "
                                             "<strong>%3</strong>. Clear the flag on that partition, or make it an "
                                             "unformatted, unencrypted partition with no mount point.",
                                             "@info" )
                                             .arg( bootDevice->deviceNode(), biosFlag, name ) );
                    }
                }
            }
        }

        if ( !root )
        {
            problems.append( tr( "Add a partition mounted at <strong>/</strong> for the system.", "@info" ) );
        }
        else if ( rootDevice->type() == Device::Type::LVM_Device )
        {
            problems.append( tr( "The partition mounted at <strong>/</strong> cannot be an LVM logical volume. "
                                 "Use a regular partition; it can still be encrypted.",
                                 "@info" ) );
        }
        else if ( rootDevice->type() == Device::Type::SoftwareRAID_Device )
        {
            problems.append( tr( "The partition mounted at <strong>/</strong> cannot be on software RAID. "
                                 "Use a regular partition; it can still be encrypted.",
                                 "@info" ) );
        }
    }

    if ( m_requireFormattedRoot && root && !PartitionInfo::format( root )
         && !Calamares::Partition::isPartitionNew( root ) )
    {
        problems.append( isEncrypted( root )
                             ? tr( "The encrypted partition mounted at <strong>/</strong> has to be formatted, and "
                                   "Format would leave it unencrypted. To keep / encrypted, delete it and create a "
                                   "new partition with <strong>Encrypt</strong> checked.",
                                   "@info" )
                             : tr( "The partition mounted at <strong>/</strong> has to be formatted. Edit it and "
                                   "choose Format instead of Keep.",
                                   "@info" ) );
    }

    return problems;
}

void
PartitionPage::updateLayoutProblems()
{
    // A revert replaces the devices on another thread, and its callback checks
    // again once it is done.
    if ( !m_revertMutex.tryLock() )
    {
        return;
    }
    const bool wasAcceptable = isLayoutAcceptable();
    m_layoutProblems = layoutProblems();
    m_revertMutex.unlock();

    if ( m_layoutProblems.isEmpty() )
    {
        m_ui->layoutProblemPanel->hide();
        m_ui->layoutProblemLabel->clear();
    }
    else
    {
        QString text = tr( "To continue, the layout needs these changes:", "@info" ) + QStringLiteral( "<ul>" );
        for ( const auto& problem : std::as_const( m_layoutProblems ) )
        {
            text += QStringLiteral( "<li>" ) + problem + QStringLiteral( "</li>" );
        }
        text += QStringLiteral( "</ul>" );
        m_ui->layoutProblemLabel->setText( text );
        m_ui->layoutProblemPanel->show();
    }

    if ( wasAcceptable != isLayoutAcceptable() )
    {
        Q_EMIT layoutAcceptableChanged( isLayoutAcceptable() );
    }
}
