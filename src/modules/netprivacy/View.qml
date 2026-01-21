/* SPDX-FileCopyrightText: 2026 VCPU
   SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

Rectangle {
    id: root
    color: "transparent"

    readonly property int macOff:    0
    readonly property int macRandom: 1
    readonly property int macVendor: 2
    readonly property int macFixed:  3
    readonly property int ip6Std:    0
    readonly property int ip6Priv:   1
    readonly property int ip6Off:    2

    SystemPalette { id: pal; colorGroup: SystemPalette.Active }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 20
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: 20

            GroupBox {
                title: qsTr("MAC Randomization")
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 8
                    ButtonGroup { id: macGrp }

                    RadioButton {
                        ButtonGroup.group: macGrp
                        text: qsTr("Off")
                        checked: config.macPolicy === root.macOff
                        onClicked: config.macPolicy = root.macOff
                    }
                    RadioButton {
                        ButtonGroup.group: macGrp
                        text: qsTr("Random")
                        checked: config.macPolicy === root.macRandom
                        onClicked: config.macPolicy = root.macRandom
                    }
                    RadioButton {
                        ButtonGroup.group: macGrp
                        text: qsTr("Vendor")
                        checked: config.macPolicy === root.macVendor
                        onClicked: config.macPolicy = root.macVendor
                    }

                    ComboBox {
                        id: vendorCombo
                        Layout.leftMargin: 24
                        Layout.preferredWidth: 240
                        visible: config.macPolicy === root.macVendor
                        textRole: "name"
                        valueRole: "id"
                        model: config.vendorList
                        onActivated: function(index) {
                            if (model && index >= 0)
                                config.selectedVendor = model[index].id
                        }
                        Component.onCompleted: vendorCombo.syncIndex()
                        Connections {
                            target: config
                            function onSelectedVendorChanged() { vendorCombo.syncIndex() }
                        }
                        function syncIndex() {
                            if (!model) return
                            for (var i = 0; i < model.length; ++i)
                                if (model[i].id === config.selectedVendor) { currentIndex = i; return }
                        }
                    }

                    RadioButton {
                        ButtonGroup.group: macGrp
                        text: qsTr("Fixed")
                        checked: config.macPolicy === root.macFixed
                        onClicked: config.macPolicy = root.macFixed
                    }

                    TextField {
                        Layout.leftMargin: 24
                        Layout.preferredWidth: 180
                        visible: config.macPolicy === root.macFixed
                        placeholderText: "XX:XX:XX:XX:XX:XX"
                        text: config.macAddress
                        font.family: "monospace"
                        onEditingFinished: config.macAddress = text
                    }

                    Text {
                        Layout.leftMargin: 24
                        visible: config.macPolicy === root.macRandom || config.macPolicy === root.macVendor
                        text: qsTr("Preview: %1").arg(config.currentPreviewMac)
                        font.italic: true
                        opacity: 0.7
                        color: pal.text
                    }

                    CheckBox {
                        Layout.leftMargin: 24
                        visible: config.macPolicy === root.macRandom || config.macPolicy === root.macVendor
                        text: qsTr("Per-connection random")
                        checked: config.perConnectionRandom
                        onToggled: config.perConnectionRandom = checked
                    }
                }
            }

            GroupBox {
                title: qsTr("IPv6")
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 8
                    ButtonGroup { id: ip6Grp }

                    RadioButton {
                        ButtonGroup.group: ip6Grp
                        text: qsTr("Standard")
                        checked: config.ipv6Mode === root.ip6Std
                        onClicked: config.ipv6Mode = root.ip6Std
                    }
                    RadioButton {
                        ButtonGroup.group: ip6Grp
                        text: qsTr("Privacy (RFC 4941)")
                        checked: config.ipv6Mode === root.ip6Priv
                        onClicked: config.ipv6Mode = root.ip6Priv
                    }
                    RadioButton {
                        ButtonGroup.group: ip6Grp
                        text: qsTr("Disable")
                        checked: config.ipv6Mode === root.ip6Off
                        onClicked: config.ipv6Mode = root.ip6Off
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }
}
