/* === This file is part of Calamares - <https://calamares.io> ===
 *
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 */
import QtQuick 2.10
import QtQuick.Controls 2.10

ComboBox {
    id: control

    property color popupBackgroundColor: "#2b2a2a"
    property color popupItemColor: "transparent"
    property color popupItemHoverColor: Qt.rgba(230 / 255, 225 / 255, 225 / 255, 0.08)
    property color popupItemSelectedColor: "#4d4b4d"
    property color popupItemSelectedHoverColor: "#5a585a"
    property color popupTextColor: "#cbc5ca"
    property color popupSelectedTextColor: "#ece6e9"
    property int popupRadius: 16
    property int popupItemRadius: 6

    delegate: ItemDelegate {
        id: itemDelegate

        width: control.popup.contentItem ? control.popup.contentItem.width : control.width
        height: 42
        hoverEnabled: true
        highlighted: control.highlightedIndex === index

        contentItem: Text {
            leftPadding: 8
            rightPadding: 8
            text: control.textAt(index)
            color: itemDelegate.highlighted ? control.popupSelectedTextColor : control.popupTextColor
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            radius: control.popupItemRadius
            color: itemDelegate.highlighted
                   ? (itemDelegate.hovered ? control.popupItemSelectedHoverColor : control.popupItemSelectedColor)
                   : itemDelegate.hovered
                     ? control.popupItemHoverColor
                     : control.popupItemColor
        }
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        height: Math.min(listView.contentHeight + topPadding + bottomPadding, 300)
        padding: 8

        background: Rectangle {
            radius: control.popupRadius
            color: control.popupBackgroundColor
        }

        contentItem: ListView {
            id: listView

            clip: true
            implicitHeight: contentHeight
            spacing: 2
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds

            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }
}
