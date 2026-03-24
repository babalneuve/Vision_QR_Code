import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11
import QtQuick.Window 2.12
import QtMultimedia 5.11
import com.hmi.qmlcomponents 1.2
import com.qrcode 1.0

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
            qrResultText.text = data
            qrResultText.visible = true
            qrHideTimer.restart()
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
                                    font.pixelSize: 14
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
