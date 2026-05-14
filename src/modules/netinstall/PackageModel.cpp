/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2017 Kyle Robbertze <kyle@aims.ac.za>
 *   SPDX-FileCopyrightText: 2017-2018 2020, Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "PackageModel.h"

#include "compat/Variant.h"
#include "utils/Logger.h"
#include "utils/Variant.h"
#include "utils/Yaml.h"

#include <QSet>

#include <functional>

/// Recursive helper for setSelections()
static void
setSelections( const QStringList& selectNames, PackageTreeItem* item )
{
    for ( int i = 0; i < item->childCount(); i++ )
    {
        auto* child = item->child( i );
        setSelections( selectNames, child );
    }
    if ( item->isGroup() && selectNames.contains( item->name() ) )
    {
        item->setSelected( Qt::CheckState::Checked );
    }
}

/** @brief Collects all the "source" values from @p groupList
 *
 * Iterates over @p groupList and returns all nonempty "source"
 * values from the maps.
 *
 */
static QStringList
collectSources( const QVariantList& groupList )
{
    QStringList sources;
    for ( const QVariant& group : groupList )
    {
        QVariantMap groupMap = group.toMap();
        if ( !groupMap[ "source" ].toString().isEmpty() )
        {
            sources.append( groupMap[ "source" ].toString() );
        }
    }

    return sources;
}

PackageModel::PackageModel( QObject* parent )
    : QAbstractItemModel( parent )
{
}

PackageModel::~PackageModel()
{
    delete m_rootItem;
}

QModelIndex
PackageModel::index( int row, int column, const QModelIndex& parent ) const
{
    if ( !m_rootItem || !hasIndex( row, column, parent ) )
    {
        return QModelIndex();
    }

    PackageTreeItem* parentItem;

    if ( !parent.isValid() )
    {
        parentItem = m_rootItem;
    }
    else
    {
        parentItem = static_cast< PackageTreeItem* >( parent.internalPointer() );
    }

    PackageTreeItem* childItem = parentItem->child( row );
    if ( childItem )
    {
        return createIndex( row, column, childItem );
    }
    else
    {
        return QModelIndex();
    }
}

QModelIndex
PackageModel::parent( const QModelIndex& index ) const
{
    if ( !m_rootItem || !index.isValid() )
    {
        return QModelIndex();
    }

    PackageTreeItem* child = static_cast< PackageTreeItem* >( index.internalPointer() );
    PackageTreeItem* parent = child->parentItem();

    if ( parent == m_rootItem )
    {
        return QModelIndex();
    }
    return createIndex( parent->row(), 0, parent );
}

int
PackageModel::rowCount( const QModelIndex& parent ) const
{
    if ( !m_rootItem || ( parent.column() > 0 ) )
    {
        return 0;
    }

    PackageTreeItem* parentItem;
    if ( !parent.isValid() )
    {
        parentItem = m_rootItem;
    }
    else
    {
        parentItem = static_cast< PackageTreeItem* >( parent.internalPointer() );
    }

    return parentItem->childCount();
}

int
PackageModel::columnCount( const QModelIndex& ) const
{
    return 2;
}

QVariant
PackageModel::data( const QModelIndex& index, int role ) const
{
    if ( !m_rootItem || !index.isValid() )
    {
        return QVariant();
    }

    PackageTreeItem* item = static_cast< PackageTreeItem* >( index.internalPointer() );
    switch ( role )
    {
    case Qt::CheckStateRole:
        return index.column() == NameColumn ? ( item->isImmutable() ? QVariant() : item->isSelected() ) : QVariant();
    case Qt::DisplayRole:
        return item->isHidden() ? QVariant() : item->data( index.column() );
    case MetaExpandRole:
        return item->isHidden() ? false : item->expandOnStart();
    default:
        return QVariant();
    }
}

bool
PackageModel::setData( const QModelIndex& index, const QVariant& value, int role )
{
    if ( !m_rootItem )
    {
        return false;
    }

    if ( role == Qt::CheckStateRole && index.isValid() )
    {
        PackageTreeItem* item = static_cast< PackageTreeItem* >( index.internalPointer() );
        const auto newState = static_cast< Qt::CheckState >( value.toInt() );
        item->setSelected( newState );

        // setSelected cascades to every descendant and bubbles a tri-state up
        // the ancestors. The view only repaints regions named in dataChanged,
        // so emit for the toggled row + every descendant + every ancestor.
        // (Original emit had row/column swapped and missed children entirely,
        // leaving child checkboxes stale until a hover triggered a repaint.)
        const QVector< int > rolesChanged { Qt::CheckStateRole };
        std::function< void( const QModelIndex& ) > emitSubtree
            = [&]( const QModelIndex& idx )
            {
                if ( !idx.isValid() )
                {
                    return;
                }
                emit dataChanged( idx, idx.sibling( idx.row(), columnCount( idx ) - 1 ), rolesChanged );
                const int rows = rowCount( idx );
                for ( int r = 0; r < rows; ++r )
                {
                    emitSubtree( this->index( r, 0, idx ) );
                }
            };
        emitSubtree( index.sibling( index.row(), 0 ) );

        for ( QModelIndex ancestor = parent( index ); ancestor.isValid(); ancestor = parent( ancestor ) )
        {
            emit dataChanged( ancestor, ancestor.sibling( ancestor.row(), columnCount( ancestor ) - 1 ), rolesChanged );
        }

        // Cross-group sync: if the same package appears in multiple
        // groups (e.g. "spotify" in both "Included Extras" and "Media
        // & Entertainment"), keep their checkboxes in lockstep so the
        // user doesn't end up with one checked and one unchecked while
        // looking at the same package. finalizeGlobalStorage also
        // dedupes by packageName so we never queue the same install
        // twice — this is purely for visual consistency.
        if ( item->isPackage() )
        {
            const QString name = item->packageName();
            PackageTreeItem::List duplicates;
            collectPackagesByName( m_rootItem, name, duplicates );
            for ( auto* dup : duplicates )
            {
                if ( dup == item || dup->isSelected() == newState )
                {
                    continue;
                }
                dup->setSelected( newState );
                // Repaint the duplicate's row and every ancestor — the
                // setSelected call above already recomputed the
                // tri-state on each ancestor, but the view doesn't know
                // until we emit dataChanged for those indices.
                const QModelIndex dupIdx = indexFor( dup );
                if ( !dupIdx.isValid() )
                {
                    continue;
                }
                emit dataChanged( dupIdx,
                                  dupIdx.sibling( dupIdx.row(), columnCount( dupIdx ) - 1 ),
                                  rolesChanged );
                for ( QModelIndex anc = parent( dupIdx ); anc.isValid(); anc = parent( anc ) )
                {
                    emit dataChanged( anc,
                                      anc.sibling( anc.row(), columnCount( anc ) - 1 ),
                                      rolesChanged );
                }
            }
        }
    }
    return true;
}

void
PackageModel::collectPackagesByName( PackageTreeItem* node,
                                     const QString& name,
                                     PackageTreeItem::List& out ) const
{
    if ( !node || name.isEmpty() )
    {
        return;
    }
    for ( int i = 0; i < node->childCount(); ++i )
    {
        auto* child = node->child( i );
        if ( !child )
        {
            continue;
        }
        if ( child->isPackage() && child->packageName() == name )
        {
            out.append( child );
        }
        // Recurse — subgroups are allowed in the YAML schema, and a
        // duplicate package could live anywhere in the tree.
        if ( child->childCount() > 0 )
        {
            collectPackagesByName( child, name, out );
        }
    }
}

QModelIndex
PackageModel::indexFor( PackageTreeItem* item ) const
{
    if ( !item || item == m_rootItem )
    {
        return QModelIndex();
    }
    PackageTreeItem* parentItem = item->parentItem();
    if ( !parentItem )
    {
        return QModelIndex();
    }
    for ( int row = 0; row < parentItem->childCount(); ++row )
    {
        if ( parentItem->child( row ) == item )
        {
            return createIndex( row, 0, item );
        }
    }
    return QModelIndex();
}

void
PackageModel::syncDuplicatePackageSelections()
{
    if ( !m_rootItem )
    {
        return;
    }

    // Pass 1: collect packageName() of every Checked leaf. Each group
    // inherits its `selected:` field from the YAML, but the child
    // packages then inherit from the parent's state — so a package in
    // a group with `selected: true` lands here, while the same package
    // in a different group with `selected: false` does not. We want
    // both to end up Checked.
    QSet< QString > checkedNames;
    std::function< void( PackageTreeItem* ) > collect = [ & ]( PackageTreeItem* node )
    {
        for ( int i = 0; i < node->childCount(); ++i )
        {
            auto* child = node->child( i );
            if ( !child )
            {
                continue;
            }
            if ( child->isPackage() && child->isSelected() == Qt::Checked
                 && !child->packageName().isEmpty() )
            {
                checkedNames.insert( child->packageName() );
            }
            if ( child->childCount() > 0 )
            {
                collect( child );
            }
        }
    };
    collect( m_rootItem );

    if ( checkedNames.isEmpty() )
    {
        return;
    }

    // Pass 2: promote any Unchecked duplicate to Checked. setSelected()
    // bubbles tri-state up the ancestor chain so groups containing the
    // duplicate also reflect the change. Caller is responsible for
    // wrapping this in beginResetModel/endResetModel, so we don't need
    // to emit dataChanged.
    std::function< void( PackageTreeItem* ) > promote = [ & ]( PackageTreeItem* node )
    {
        for ( int i = 0; i < node->childCount(); ++i )
        {
            auto* child = node->child( i );
            if ( !child )
            {
                continue;
            }
            if ( child->isPackage() && child->isSelected() != Qt::Checked
                 && checkedNames.contains( child->packageName() ) )
            {
                child->setSelected( Qt::Checked );
            }
            if ( child->childCount() > 0 )
            {
                promote( child );
            }
        }
    };
    promote( m_rootItem );
}

Qt::ItemFlags
PackageModel::flags( const QModelIndex& index ) const
{
    if ( !m_rootItem || !index.isValid() )
    {
        return Qt::ItemFlags();
    }
    if ( index.column() == NameColumn )
    {
        PackageTreeItem* item = static_cast< PackageTreeItem* >( index.internalPointer() );
        if ( item->isImmutable() || item->isNoncheckable() )
        {
            return QAbstractItemModel::flags( index );  //Qt::NoItemFlags;
        }
        return Qt::ItemIsUserCheckable | QAbstractItemModel::flags( index );
    }
    return QAbstractItemModel::flags( index );
}

QVariant
PackageModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
    if ( orientation == Qt::Horizontal && role == Qt::DisplayRole )
    {
        return ( section == NameColumn ) ? tr( "Name" ) : tr( "Description" );
    }
    return QVariant();
}

void
PackageModel::setSelections( const QStringList& selectNames )
{
    if ( m_rootItem )
    {
        ::setSelections( selectNames, m_rootItem );
    }
}

// Recursively unchecks every item in the tree. Helper for the public
// PackageModel::clearSelections() below; kept as a free function to
// mirror the file-local ::setSelections() helper above.
static void
clearSelections( PackageTreeItem* item )
{
    for ( int i = 0; i < item->childCount(); i++ )
    {
        clearSelections( item->child( i ) );
    }
    item->setSelected( Qt::CheckState::Unchecked );
}

void
PackageModel::clearSelections()
{
    if ( m_rootItem )
    {
        ::clearSelections( m_rootItem );
    }
}

PackageTreeItem::List
PackageModel::getPackages() const
{
    if ( !m_rootItem )
    {
        return PackageTreeItem::List();
    }

    auto items = getItemPackages( m_rootItem );
    for ( auto package : m_hiddenItems )
    {
        if ( package->hiddenSelected() )
        {
            items.append( getItemPackages( package ) );
        }
    }
    return items;
}

PackageTreeItem::List
PackageModel::getItemPackages( PackageTreeItem* item ) const
{
    PackageTreeItem::List selectedPackages;
    for ( int i = 0; i < item->childCount(); i++ )
    {
        auto* child = item->child( i );
        if ( child->isSelected() == Qt::Unchecked )
        {
            continue;
        }

        if ( child->isPackage() )  // package
        {
            selectedPackages.append( child );
        }
        else
        {
            selectedPackages.append( getItemPackages( child ) );
        }
    }
    return selectedPackages;
}

void
PackageModel::setupModelData( const QVariantList& groupList, PackageTreeItem* parent )
{
    for ( const auto& group : groupList )
    {
        QVariantMap groupMap = group.toMap();
        if ( groupMap.isEmpty() )
        {
            continue;
        }

        PackageTreeItem* item = new PackageTreeItem( groupMap, PackageTreeItem::GroupTag { parent } );
        if ( groupMap.contains( "selected" ) )
        {
            item->setSelected( Calamares::getBool( groupMap, "selected", false ) ? Qt::Checked : Qt::Unchecked );
        }
        if ( groupMap.contains( "packages" ) )
        {
            for ( const auto& packageName : groupMap.value( "packages" ).toList() )
            {
                if ( Calamares::typeOf( packageName ) == Calamares::StringVariantType )
                {
                    item->appendChild( new PackageTreeItem( packageName.toString(), item ) );
                }
                else
                {
                    QVariantMap m = packageName.toMap();
                    if ( !m.isEmpty() )
                    {
                        item->appendChild( new PackageTreeItem( m, PackageTreeItem::PackageTag { item } ) );
                    }
                }
            }
            if ( !item->childCount() )
            {
                cWarning() << "*packages* under" << item->name() << "is empty.";
            }
        }
        if ( groupMap.contains( "subgroups" ) )
        {
            bool haveWarned = false;
            const auto& subgroupValue = groupMap.value( "subgroups" );
            if ( !subgroupValue.canConvert< QVariantList >() )
            {
                cWarning() << "*subgroups* under" << item->name() << "is not a list.";
                haveWarned = true;
            }

            QVariantList subgroups = groupMap.value( "subgroups" ).toList();
            if ( !subgroups.isEmpty() )
            {
                setupModelData( subgroups, item );
                // The children might be checked while the parent isn't (yet).
                // Children are added to their parent (below) without affecting
                // the checked-state -- do it manually. Items with subgroups
                // but no children have only hidden children -- those get
                // handled specially.
                if ( item->childCount() > 0 )
                {
                    item->updateSelected();
                }
            }
            else
            {
                if ( !haveWarned )
                {
                    cWarning() << "*subgroups* list under" << item->name() << "is empty.";
                }
            }
        }
        if ( item->isHidden() )
        {
            m_hiddenItems.append( item );
            if ( !item->isSelected() )
            {
                cWarning() << "Item" << ( item->parentItem() ? item->parentItem()->name() : QString() ) << '.'
                           << item->name() << "is hidden, but not selected.";
            }
        }
        else
        {
            item->setCheckable( true );
            parent->appendChild( item );
        }
    }
}

void
PackageModel::setupModelData( const QVariantList& l )
{
    beginResetModel();
    delete m_rootItem;
    m_rootItem = new PackageTreeItem();
    setupModelData( l, m_rootItem );
    // Promote unchecked duplicates of any checked package — keeps the
    // initial UI state consistent across groups when the same package
    // appears in more than one place (e.g. `spotify` in both "Included
    // Extras" and "Media & Entertainment"). Mirrors setData()'s runtime
    // cross-group sync.
    syncDuplicatePackageSelections();
    endResetModel();
}

void
PackageModel::appendModelData( const QVariantList& groupList )
{
    if ( m_rootItem )
    {
        beginResetModel();

        const QStringList sources = collectSources( groupList );

        if ( !sources.isEmpty() )
        {
            // Prune any existing data from the same source
            QList< int > removeList;
            for ( int i = 0; i < m_rootItem->childCount(); i++ )
            {
                PackageTreeItem* child = m_rootItem->child( i );
                if ( sources.contains( child->source() ) )
                {
                    removeList.insert( 0, i );
                }
            }
            for ( const int& item : std::as_const( removeList ) )
            {
                m_rootItem->removeChild( item );
            }
        }

        // Add the new data to the model
        setupModelData( groupList, m_rootItem );
        // Re-run the duplicate sync after the merge — a newly appended
        // group may add a Checked package whose duplicate already lives
        // in an earlier group, or vice versa.
        syncDuplicatePackageSelections();

        endResetModel();
    }
}
