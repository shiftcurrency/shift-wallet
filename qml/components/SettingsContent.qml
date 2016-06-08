import QtQuick 2.0
import QtQuick.Controls 1.0
import QtQuick.Dialogs 1.0

TabView {
    Tab {
        title: qsTr("Basic")

        Column {
            anchors.margins: 0.2 * dpi
            anchors.fill: parent
            spacing: 0.1 * dpi

            Row {
                id: rowGshiftDatadir
                width: parent.width

                Label {
                    id: gshiftDDLabel
                    text: "Gshift Data Directory: "
                }

                TextField {
                    id: gshiftDDField
                    width: parent.width - gshiftDDButton.width - gshiftDDLabel.width
                    text: settings.value("gshift/datadir", "")
                    onTextChanged: {
                        settings.setValue("gshift/datadir", gshiftDDField.text)
                    }
                }

                Button {
                    id: gshiftDDButton
                    text: qsTr("Choose")

                    onClicked: {
                        ddFileDialog.open()
                    }
                }

                FileDialog {
                    id: ddFileDialog
                    title: qsTr("Gshift data directory")
                    selectFolder: true
                    selectExisting: true
                    selectMultiple: false

                    onAccepted: {
                        gshiftDDField.text = ddFileDialog.fileUrl.toString().replace(/^(file:\/{3})/,""); // fugly but gotta love QML on this one
                    }
                }
            }

            Row {
                width: parent.width

                Label {
                    text: qsTr("Update interval (s): ")
                }

                SpinBox {
                    id: intervalSpinBox
                    width: 1 * dpi
                    minimumValue: 5
                    maximumValue: 60

                    value: settings.value("ipc/interval", 10)
                    onValueChanged: {
                        settings.setValue("ipc/interval", intervalSpinBox.value)
                        ipc.setInterval(intervalSpinBox.value * 1000)
                    }
                }
            }

        }
    }

    Tab {
        title: qsTr("Advanced")

        Column {
            anchors.margins: 0.2 * dpi
            anchors.fill: parent
            spacing: 0.1 * dpi

            Row {
                id: rowGshiftPath
                width: parent.width

                Label {
                    id: gshiftPathLabel
                    text: "Gshift path: "
                }

                TextField {
                    id: gshiftPathField
                    width: parent.width - gshiftPathLabel.width - gshiftPathButton.width
                    text: settings.value("gshift/path", "")
                    onTextChanged: {
                        settings.setValue("gshift/path", gshiftPathField.text)
                    }
                }

                Button {
                    id: gshiftPathButton
                    text: qsTr("Choose")

                    onClicked: {
                        gshiftFileDialog.open()
                    }
                }

                FileDialog {
                    id: gshiftFileDialog
                    title: qsTr("Gshift executable")
                    selectFolder: false
                    selectExisting: true
                    selectMultiple: false

                    onAccepted: {
                        gshiftPathField.text = gshiftFileDialog.fileUrl.toString().replace(/^(file:\/{3})/,""); // fugly but gotta love QML on this one
                    }
                }
            }

            // TODO: rename to infodialog
            ErrorDialog {
                id: confirmDialog
                title: qsTr("Warning")
                msg: qsTr("Changing the chain requires a restart of ShiftWallet.")
            }

            Row {
                id: rowGshiftTestnet
                width: parent.width

                Label {
                    id: gshiftTestnetLabel
                    text: "Testnet: "
                }

                CheckBox {
                    id: gshiftTestnetCheck
                    checked: settings.value("gshift/testnet", false)
                    onClicked: {
                        settings.setValue("gshift/testnet", gshiftTestnetCheck.checked)
                        confirmDialog.show()
                    }
                }
            }

            Row {
                id: rowGshiftArgs
                width: parent.width

                Label {
                    id: gshiftArgsLabel
                    text: "Additional Gshift args: "
                }

                TextField {
                    id: gshiftArgsField
                    width: parent.width - gshiftArgsLabel.width
                    text: settings.value("gshift/args", "--fast --cache 512")
                    onTextChanged: {
                        settings.setValue("gshift/args", gshiftArgsField.text)
                    }
                }
            }

        }
    }

}
