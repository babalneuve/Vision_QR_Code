import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11
import QtQuick.Window 2.12
import QtMultimedia 5.11
import com.hmi.qmlcomponents 1.2
import com.qrcode 1.0
import com.datetime 1.0

/* Defines the main window which will be shown full screen */
Window {

    id: window
    objectName: "main_window"
    width: Screen.width
    height: Screen.height
    visible: true
    /* Remove frames around the surface as you would see on a classic Desktop */
    flags: (Qt.FramelessWindowHint | Qt.Window | Qt.NoDropShadowWindowHint)

    property bool isVision307: (Screen.width == 800 && Screen.height == 480)
    property real displayScaleX: isVision307 ? (800/1280) : 1
    property real displayScaleY: isVision307 ? (480/800) : 1

    // QR Code Reader for camera1 (only camera1 used)
    QrCodeReader {
        id: qrReader1
        target: camera1
        scanInterval: 300
        Component.onCompleted: scanning = true

        onQrCodeDetected: {
            console.log("QR Code detected on Camera 1: " + data)

            // Check if the QR code contains at least one recognized LED command
            var lines = data.split('\n')
            var hasValidCommand = false
            for (var i = 0; i < lines.length; i++) {
                var cmd = lines[i].trim()
                if (cmd === "LED 1 : ON" || cmd === "LED 1 : OFF" ||
                    cmd === "LED 2 : ON" || cmd === "LED 2 : OFF") {
                    hasValidCommand = true
                    break
                }
            }

            if (hasValidCommand) {
                qrResultText.text = data
                qrResultText.visible = true
                qrHideTimer.restart()
            } else {
                // Pause scanning and show invalid message until user acknowledges
                qrReader1.scanning = false
                invalidOverlay.visible = true
            }
        }
    }

    // Timer to hide QR result after a delay
    Timer {
        id: qrHideTimer
        interval: 5000
        onTriggered: {
            qrResultText.visible = false
            qrResultText.text = ""
        }
    }


    Item {
        id: itemId
        objectName: "itemId"
        width: 1280
        height: 800

        transform: Scale { origin.x: 0; origin.y: 0; xScale: displayScaleX; yScale: displayScaleY}

        // USB icon — bottom-right corner, visible when a USB drive is mounted
        Image {
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.margins: 10
            width: 40
            height: 40
            sourceSize.width: 40
            sourceSize.height: 40
            source: "qrc:/Images/usb.svg"
            visible: usbWatcher.usbPresent
            z: 5

            MouseArea {
                anchors.fill: parent
                onClicked: usbPopup.open()
            }
        }

        // Popup USB — export des logs vers la clé USB
        Rectangle {
            id: usbPopup
            anchors.centerIn: parent
            width: 420
            height: 220
            color: "#f5f5f5"
            border.color: "#0077cc"
            border.width: 2
            radius: 10
            visible: false
            z: 20

            function open()  { visible = true;  exportStatus.text = ""; exportStatus.visible = false }
            function close() { visible = false }

            Connections {
                target: usbWatcher
                onExportResult: {
                    exportStatus.color   = success ? "#006600" : "#cc0000"
                    exportStatus.text    = message
                    exportStatus.visible = true
                    if (success)
                        closeTimer.start()
                }
            }

            Timer {
                id: closeTimer
                interval: 2000
                onTriggered: usbPopup.close()
            }

            Column {
                anchors.centerIn: parent
                spacing: 20

                Text {
                    text: qsTr("Exporter les logs")
                    font.pixelSize: 18
                    font.bold: true
                    color: "#333333"
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    id: exportStatus
                    anchors.horizontalCenter: parent.horizontalCenter
                    wrapMode: Text.WordWrap
                    font.pixelSize: 14
                    visible: false
                }

                Row {
                    spacing: 16
                    anchors.horizontalCenter: parent.horizontalCenter

                    Rectangle {
                        width: 120; height: 44; radius: 6
                        color: cancelUsbBtn.pressed ? "#999999" : "#bbbbbb"
                        Text { anchors.centerIn: parent; text: qsTr("Annuler"); font.pixelSize: 15; color: "#333333" }
                        MouseArea { id: cancelUsbBtn; anchors.fill: parent; onClicked: usbPopup.close() }
                    }

                    Rectangle {
                        width: 180; height: 44; radius: 6
                        color: exportBtn.pressed ? "#005500" : "#007700"
                        Text { anchors.centerIn: parent; text: qsTr("Exporter les logs"); font.pixelSize: 15; color: "white" }
                        MouseArea {
                            id: exportBtn
                            anchors.fill: parent
                            onClicked: usbWatcher.exportLogs("/data/vision_qr_code/logs")
                        }
                    }
                }
            }
        }

        // Date and time display + set button — bottom-left corner
        Row {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.margins: 10
            spacing: 10
            z: 5

            Text {
                id: dateTimeText
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: 20
                color: "#333333"

                function formatted() {
                    var d = new Date()
                    var dd   = ("0" + d.getDate()).slice(-2)
                    var mm   = ("0" + (d.getMonth() + 1)).slice(-2)
                    var yyyy = d.getFullYear()
                    var hh   = ("0" + d.getHours()).slice(-2)
                    var min  = ("0" + d.getMinutes()).slice(-2)
                    return dd + "/" + mm + "/" + yyyy + " " + hh + ":" + min
                }

                text: formatted()

                Timer {
                    interval: 30000
                    running: true
                    repeat: true
                    onTriggered: dateTimeText.text = dateTimeText.formatted()
                }
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 34
                height: 34
                radius: 5
                color: setDateBtn.pressed ? "#0055aa" : "#0077cc"

                Text {
                    anchors.centerIn: parent
                    text: "✎"
                    font.pixelSize: 18
                    color: "white"
                }

                MouseArea {
                    id: setDateBtn
                    anchors.fill: parent
                    onClicked: {
                        var d = new Date()
                        dayInput.value   = d.getDate()
                        monthInput.value = d.getMonth() + 1
                        yearInput.value  = d.getFullYear()
                        hourInput.value  = d.getHours()
                        minInput.value   = d.getMinutes()
                        dateTimePopup.open()
                    }
                }
            }
        }

        // Popup to manually set date and time
        Rectangle {
            id: dateTimePopup
            anchors.centerIn: parent
            width: 580
            height: 310
            color: "#f5f5f5"
            border.color: "#0077cc"
            border.width: 2
            radius: 10
            visible: false
            z: 20

            function open()  { visible = true  }
            function close() { visible = false }

            Column {
                anchors.centerIn: parent
                spacing: 22

                Text {
                    text: qsTr("Régler la date et l'heure")
                    font.pixelSize: 18
                    font.bold: true
                    color: "#333333"
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                // Date row: dd / mm / yyyy
                Row {
                    spacing: 10
                    anchors.horizontalCenter: parent.horizontalCenter

                    Text { text: qsTr("Date :"); font.pixelSize: 16; anchors.verticalCenter: parent.verticalCenter }

                    // Day [−][dd][+]
                    Item {
                        id: dayInput
                        property int value: 1
                        width: 116; height: 44
                        Row {
                            anchors.fill: parent
                            Rectangle {
                                width: 36; height: parent.height; radius: 4
                                color: dayMinus.pressed ? "#aaaaaa" : "#dddddd"
                                Text { anchors.centerIn: parent; text: "−"; font.pixelSize: 22; font.bold: true; color: "#333" }
                                MouseArea { id: dayMinus; anchors.fill: parent; onClicked: if (dayInput.value > 1) dayInput.value-- }
                            }
                            Rectangle {
                                width: 44; height: parent.height
                                color: "white"; border.color: "#bbbbbb"
                                Text { anchors.centerIn: parent; text: ("0" + dayInput.value).slice(-2); font.pixelSize: 17; color: "#222" }
                            }
                            Rectangle {
                                width: 36; height: parent.height; radius: 4
                                color: dayPlus.pressed ? "#aaaaaa" : "#dddddd"
                                Text { anchors.centerIn: parent; text: "+"; font.pixelSize: 22; font.bold: true; color: "#333" }
                                MouseArea { id: dayPlus; anchors.fill: parent; onClicked: if (dayInput.value < 31) dayInput.value++ }
                            }
                        }
                    }

                    Text { text: "/"; font.pixelSize: 16; anchors.verticalCenter: parent.verticalCenter }

                    // Month [−][mm][+]
                    Item {
                        id: monthInput
                        property int value: 1
                        width: 116; height: 44
                        Row {
                            anchors.fill: parent
                            Rectangle {
                                width: 36; height: parent.height; radius: 4
                                color: monthMinus.pressed ? "#aaaaaa" : "#dddddd"
                                Text { anchors.centerIn: parent; text: "−"; font.pixelSize: 22; font.bold: true; color: "#333" }
                                MouseArea { id: monthMinus; anchors.fill: parent; onClicked: if (monthInput.value > 1) monthInput.value-- }
                            }
                            Rectangle {
                                width: 44; height: parent.height
                                color: "white"; border.color: "#bbbbbb"
                                Text { anchors.centerIn: parent; text: ("0" + monthInput.value).slice(-2); font.pixelSize: 17; color: "#222" }
                            }
                            Rectangle {
                                width: 36; height: parent.height; radius: 4
                                color: monthPlus.pressed ? "#aaaaaa" : "#dddddd"
                                Text { anchors.centerIn: parent; text: "+"; font.pixelSize: 22; font.bold: true; color: "#333" }
                                MouseArea { id: monthPlus; anchors.fill: parent; onClicked: if (monthInput.value < 12) monthInput.value++ }
                            }
                        }
                    }

                    Text { text: "/"; font.pixelSize: 16; anchors.verticalCenter: parent.verticalCenter }

                    // Year [−][yyyy][+]
                    Item {
                        id: yearInput
                        property int value: 2025
                        width: 136; height: 44
                        Row {
                            anchors.fill: parent
                            Rectangle {
                                width: 36; height: parent.height; radius: 4
                                color: yearMinus.pressed ? "#aaaaaa" : "#dddddd"
                                Text { anchors.centerIn: parent; text: "−"; font.pixelSize: 22; font.bold: true; color: "#333" }
                                MouseArea { id: yearMinus; anchors.fill: parent; onClicked: if (yearInput.value > 2000) yearInput.value-- }
                            }
                            Rectangle {
                                width: 64; height: parent.height
                                color: "white"; border.color: "#bbbbbb"
                                Text { anchors.centerIn: parent; text: yearInput.value; font.pixelSize: 17; color: "#222" }
                            }
                            Rectangle {
                                width: 36; height: parent.height; radius: 4
                                color: yearPlus.pressed ? "#aaaaaa" : "#dddddd"
                                Text { anchors.centerIn: parent; text: "+"; font.pixelSize: 22; font.bold: true; color: "#333" }
                                MouseArea { id: yearPlus; anchors.fill: parent; onClicked: if (yearInput.value < 2099) yearInput.value++ }
                            }
                        }
                    }
                }

                // Time row: hh : mm
                Row {
                    spacing: 10
                    anchors.horizontalCenter: parent.horizontalCenter

                    Text { text: qsTr("Heure :"); font.pixelSize: 16; anchors.verticalCenter: parent.verticalCenter }

                    // Hour [−][hh][+]
                    Item {
                        id: hourInput
                        property int value: 0
                        width: 116; height: 44
                        Row {
                            anchors.fill: parent
                            Rectangle {
                                width: 36; height: parent.height; radius: 4
                                color: hourMinus.pressed ? "#aaaaaa" : "#dddddd"
                                Text { anchors.centerIn: parent; text: "−"; font.pixelSize: 22; font.bold: true; color: "#333" }
                                MouseArea { id: hourMinus; anchors.fill: parent; onClicked: if (hourInput.value > 0) hourInput.value-- }
                            }
                            Rectangle {
                                width: 44; height: parent.height
                                color: "white"; border.color: "#bbbbbb"
                                Text { anchors.centerIn: parent; text: ("0" + hourInput.value).slice(-2); font.pixelSize: 17; color: "#222" }
                            }
                            Rectangle {
                                width: 36; height: parent.height; radius: 4
                                color: hourPlus.pressed ? "#aaaaaa" : "#dddddd"
                                Text { anchors.centerIn: parent; text: "+"; font.pixelSize: 22; font.bold: true; color: "#333" }
                                MouseArea { id: hourPlus; anchors.fill: parent; onClicked: if (hourInput.value < 23) hourInput.value++ }
                            }
                        }
                    }

                    Text { text: ":"; font.pixelSize: 16; anchors.verticalCenter: parent.verticalCenter }

                    // Minute [−][mm][+]
                    Item {
                        id: minInput
                        property int value: 0
                        width: 116; height: 44
                        Row {
                            anchors.fill: parent
                            Rectangle {
                                width: 36; height: parent.height; radius: 4
                                color: minMinus.pressed ? "#aaaaaa" : "#dddddd"
                                Text { anchors.centerIn: parent; text: "−"; font.pixelSize: 22; font.bold: true; color: "#333" }
                                MouseArea { id: minMinus; anchors.fill: parent; onClicked: if (minInput.value > 0) minInput.value-- }
                            }
                            Rectangle {
                                width: 44; height: parent.height
                                color: "white"; border.color: "#bbbbbb"
                                Text { anchors.centerIn: parent; text: ("0" + minInput.value).slice(-2); font.pixelSize: 17; color: "#222" }
                            }
                            Rectangle {
                                width: 36; height: parent.height; radius: 4
                                color: minPlus.pressed ? "#aaaaaa" : "#dddddd"
                                Text { anchors.centerIn: parent; text: "+"; font.pixelSize: 22; font.bold: true; color: "#333" }
                                MouseArea { id: minPlus; anchors.fill: parent; onClicked: if (minInput.value < 59) minInput.value++ }
                            }
                        }
                    }
                }

                // Buttons
                Row {
                    spacing: 16
                    anchors.horizontalCenter: parent.horizontalCenter

                    Rectangle {
                        width: 120; height: 44; radius: 6
                        color: cancelBtn.pressed ? "#999999" : "#bbbbbb"
                        Text { anchors.centerIn: parent; text: qsTr("Annuler"); font.pixelSize: 15; color: "#333333" }
                        MouseArea { id: cancelBtn; anchors.fill: parent; onClicked: dateTimePopup.close() }
                    }

                    Rectangle {
                        width: 120; height: 44; radius: 6
                        color: validateBtn.pressed ? "#005500" : "#007700"
                        Text { anchors.centerIn: parent; text: qsTr("Valider"); font.pixelSize: 15; color: "white" }
                        MouseArea {
                            id: validateBtn
                            anchors.fill: parent
                            onClicked: {
                                var ok = DateTimeHandler.setDateTime(
                                    dayInput.value, monthInput.value, yearInput.value,
                                    hourInput.value, minInput.value)
                                if (ok) {
                                    dateTimeText.text = dateTimeText.formatted()
                                    dateTimePopup.close()
                                }
                            }
                        }
                    }
                }
            }
        }

        // Overlay shown when an unrecognized QR code is scanned — dismissed manually
        Rectangle {
            id: invalidOverlay
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width * 2 / 3
            height: 180
            color: "#cc0000"
            radius: 10
            visible: false
            z: 10

            Column {
                anchors.centerIn: parent
                spacing: 20

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("QR Code Invalide")
                    font.pixelSize: 32
                    font.bold: true
                    color: "white"
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 200
                    height: 60
                    radius: 8
                    color: retryButton.pressed ? "#005500" : "#007700"

                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Recommencer")
                        font.pixelSize: 22
                        font.bold: true
                        color: "white"
                    }

                    MouseArea {
                        id: retryButton
                        anchors.fill: parent
                        onClicked: {
                            invalidOverlay.visible = false
                            qrReader1.scanning = true
                        }
                    }
                }
            }
        }

        /* Horizontal layout: camera on left (2/3), controls on right (1/3) */
        RowLayout {
            anchors.fill: parent
            spacing: 20

            // Camera area (2/3 of space)
            Item {
                Layout.preferredWidth: (parent.width * 2/3) - 10
                Layout.fillHeight: true

                // Camera centered in its area
                HmiDigitalCamera {
                    id: camera1
                    objectName: "camera1"
                    width: 800
                    height: 600

                    // Center camera in its allocated area
                    x: (parent.width - width) / 2
                    y: (parent.height - height) / 2

                    camera_info: {
                        "ip": "10.100.30.3",
                        "port": 50003,
                        "path": "/data/camera1_%d.mp4",
                        "recording_filename": "camera1"
                    }
                }
            }

            // Control panel on right (1/3 of space)
            Rectangle {
                Layout.preferredWidth: (parent.width / 3) - 10
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                color: "#f0f0f0"
                radius: 10

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 20

                    // Title
                    Text {
                        text: qsTr("QR Code Control")
                        font.pixelSize: 20
                        font.bold: true
                        color: "#333333"
                        Layout.alignment: Qt.AlignHCenter
                    }

                    // Separator
                    Rectangle {
                        Layout.preferredWidth: parent.width
                        Layout.preferredHeight: 2
                        color: "#cccccc"
                    }

                    // Scan status - full width with centered text
                    ColumnLayout {
                        spacing: 5
                        Layout.fillWidth: true

                        Text {
                            text: qsTr("Scan Status:")
                            font.pixelSize: 16
                            font.bold: true
                            color: "#555555"
                            Layout.leftMargin: 10
                            Layout.rightMargin: 10
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 70
                            Layout.leftMargin: 10
                            Layout.rightMargin: 10
                            color: qrReader1.scanning ? "#d4edda" : "#f8d7da"
                            radius: 5
                            border.color: qrReader1.scanning ? "#c3e6cb" : "#f5c6cb"
                            border.width: 1

                            // Centered content
                            Item {
                                anchors.fill: parent

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 10

                                    Rectangle {
                                        width: 16
                                        height: 16
                                        radius: 8
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: qrReader1.scanning ? "#28a745" : "#dc3545"
                                    }

                                    Text {
                                        text: qrReader1.scanning ? qsTr("Scanning Active") : qsTr("Scan Stopped")
                                        font.pixelSize: 16
                                        font.bold: true
                                        color: qrReader1.scanning ? "#155724" : "#721c24"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }
                        }
                    }

                    // Control button - full width
                    ColumnLayout {
                        spacing: 5
                        Layout.fillWidth: true

                        Text {
                            text: qsTr("Control:")
                            font.pixelSize: 16
                            font.bold: true
                            color: "#555555"
                            Layout.leftMargin: 10
                            Layout.rightMargin: 10
                        }

                        Button {
                            id: qrScanToggle
                            Layout.fillWidth: true
                            Layout.preferredHeight: 80
                            Layout.leftMargin: 10
                            Layout.rightMargin: 10
                            text: qrReader1.scanning ? qsTr("■ STOP SCAN") : qsTr("▶ START SCAN")
                            font.pixelSize: 16
                            font.bold: true
                            font.capitalization: Font.AllUppercase

                            background: Rectangle {
                                anchors.fill: parent
                                color: qrReader1.scanning ? "#dc3545" : "#28a745"
                                radius: 5
                                border.color: qrReader1.scanning ? "#c82333" : "#218838"
                                border.width: 1
                            }

                            contentItem: Text {
                                text: parent.text
                                font: parent.font
                                color: "white"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                wrapMode: Text.WordWrap
                                elide: Text.ElideNone
                            }

                            onClicked: {
                                qrReader1.scanning = !qrReader1.scanning
                            }
                        }
                    }

                    // QR Code result - always visible area, temporary text
                    ColumnLayout {
                        spacing: 5
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Text {
                            text: qsTr("Last QR Code Detected:")
                            font.pixelSize: 16
                            font.bold: true
                            color: "#555555"
                            Layout.leftMargin: 10
                            Layout.rightMargin: 10
                        }

                        Rectangle {
                            id: qrResultRect
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.leftMargin: 10
                            Layout.rightMargin: 10
                            color: "#e0ffe0"
                            border.color: "#00aa00"
                            border.width: 2
                            radius: 5
                            visible: true

                            ScrollView {
                                anchors.fill: parent
                                anchors.margins: 10
                                clip: true

                                TextArea {
                                    id: qrResultText
                                    text: ""
                                    font.pixelSize: 20
                                    font.family: "Courier"
                                    wrapMode: Text.WordWrap
                                    readOnly: true
                                    background: null
                                    color: "#006600"
                                    visible: false

                                    Text {
                                        anchors.centerIn: parent
                                        text: qsTr("No QR code detected")
                                        font.pixelSize: 14
                                        color: "#999999"
                                        visible: !parent.visible && parent.text === ""
                                    }
                                }
                            }
                        }
                    }

                    // Additional information
                    Text {
                        text: qsTr("Result displayed for 5 seconds")
                        font.pixelSize: 12
                        color: "#999999"
                        Layout.alignment: Qt.AlignHCenter
                        Layout.bottomMargin: 10
                    }
                }
            }
        }
    }
}
