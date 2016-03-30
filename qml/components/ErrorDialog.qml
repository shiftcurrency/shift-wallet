/*
    This file is part of SHIFT Wallet based on etherwall.
    SHIFT Wallet based on etherwall is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    SHIFT Wallet based on etherwall is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with SHIFT Wallet based on etherwall. If not, see <http://www.gnu.org/licenses/>.
*/
/** @file ErrorDialog.qml
 * @author Ales Katona <almindor@gmail.com>
 * @date 2015
 *
 * Error dialog
 */

import QtQuick 2.0
import QtQuick.Controls 1.1

BaseDialog {
    title: qsTr("Error")

    Button {
        text: "OK"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10
        onClicked: {
           close()
           accepted()
        }
    }
}
