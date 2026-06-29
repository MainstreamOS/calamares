/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-FileCopyrightText: 2017 Kyle Robbertze <kyle@aims.ac.za>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#ifndef PACKAGEMODEL_H
#define PACKAGEMODEL_H

#include "PackageTreeItem.h"

#include <QAbstractItemModel>
#include <QHash>
#include <QObject>
#include <QString>

namespace YAML
{
class Node;
}  // namespace YAML

class PackageModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    // Names for columns (unused in the code)
    static constexpr const int NameColumn = 0;
    static constexpr const int DescriptionColumn = 1;

    /* The only interesting roles are DisplayRole (with text depending
     * on the column, and MetaExpandRole which tells if an index
     * should be initially expanded.
     */
    static constexpr const int MetaExpandRole = Qt::UserRole + 1;

    explicit PackageModel( QObject* parent = nullptr );
    ~PackageModel() override;

    void setupModelData( const QVariantList& l );

    QVariant data( const QModelIndex& index, int role ) const override;
    bool setData( const QModelIndex& index, const QVariant& value, int role = Qt::EditRole ) override;
    Qt::ItemFlags flags( const QModelIndex& index ) const override;

    QModelIndex index( int row, int column, const QModelIndex& parent = QModelIndex() ) const override;
    QModelIndex parent( const QModelIndex& index ) const override;

    QVariant headerData( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const override;
    int rowCount( const QModelIndex& parent = QModelIndex() ) const override;
    int columnCount( const QModelIndex& parent = QModelIndex() ) const override;

    /** @brief Sets the checked flag on matching groups in the tree
     *
     * Recursively traverses the tree pointed to by m_rootItem and
     * checks if a group name matches any of the items in @p selectNames.
     * If a match is found, set check the box for that group and it's children.
     *
     * Individual packages will not be matched.
     *
     */
    void setSelections( const QStringList& selectNames );

    /** @brief Recursively unchecks every group and package in the tree.
     *
     * Use this when an upstream module (e.g. installmethod's "OS Only"
     * choice) has decided that nothing from netinstall should be
     * installed. Pairs naturally with `setSelections()` when the
     * upstream module wants to choose an exact set of groups to check
     * rather than additively layering selections on top of the model's
     * defaults.
     */
    void clearSelections();

    PackageTreeItem::List getPackages() const;
    PackageTreeItem::List getItemPackages( PackageTreeItem* item ) const;

    /** @brief Appends groups to the tree
     *
     * Uses the data from @p groupList to add elements to the
     * existing tree that m_rootItem points to.  If m_rootItem
     * is not valid, it does nothing
     *
     * Before adding anything to the model, it ensures that there
     * is no existing data from the same source.  If there is, that
     * data is pruned first
     *
     */
    void appendModelData( const QVariantList& groupList );

private:
    friend class ItemTests;

    void setupModelData( const QVariantList& l, PackageTreeItem* parent );

    /** @brief Walk the tree and collect every package-leaf whose
     *  packageName() equals @p name.
     *
     * Used by setData() to synchronise checkbox state across groups for
     * packages that appear in more than one place (e.g. a "Spotify"
     * entry in both "Included Extras" and "Media & Entertainment").
     */
    void collectPackagesByName( PackageTreeItem* node,
                                const QString& name,
                                PackageTreeItem::List& out ) const;

    /** @brief Rebuild the packageName() → items index from the tree.
     *
     * Populates m_packagesByName by walking the whole tree once. Must be
     * called at every point the tree structure changes (inside the
     * beginResetModel/endResetModel brackets of setupModelData() and
     * appendModelData()), because the index holds raw PackageTreeItem*
     * and appendModelData() prunes/deletes children — a stale index would
     * dangle.
     */
    void rebuildPackageNameIndex();

    /** @brief Emit dataChanged across all columns of @p idx's row. */
    void emitRowChanged( const QModelIndex& idx );

    /** @brief Emit dataChanged for @p idx's row and every ancestor row. */
    void emitRowAndAncestorsChanged( const QModelIndex& idx );

    /** @brief Build the QModelIndex pointing at @p item.
     *
     * Walks up to the root via parentItem() to determine each row, then
     * builds a column-0 index. Returns invalid for the root or for
     * orphan items.
     */
    QModelIndex indexFor( PackageTreeItem* item ) const;

    /** @brief Sync initial check state across duplicate packages.
     *
     * After setupModelData() finishes building the tree, walk it once to
     * collect packageName() for every Checked leaf, then walk again and
     * promote any Unchecked leaf with a matching name to Checked. The
     * setSelected() call on each promoted leaf bubbles a tri-state up
     * its ancestors so groups containing the duplicate also reflect the
     * change.
     *
     * This makes init-time match the runtime behaviour in setData()'s
     * cross-group sync — a package selected by being in "Included Extras"
     * appears checked in every other group it also lives in.
     */
    void syncDuplicatePackageSelections();

    PackageTreeItem* m_rootItem = nullptr;
    PackageTreeItem::List m_hiddenItems;
    QHash< QString, PackageTreeItem::List > m_packagesByName;
};

#endif  // PACKAGEMODEL_H
