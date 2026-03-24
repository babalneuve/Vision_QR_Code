import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11
import QtQuick.Window 2.12
import QtMultimedia 5.11
import com.qrcode 1.0

Window {
    id: window
    width: 800
    height: 600
    visible: true
    title: qsTr("Vision QR Code - PC Webcam")

    property bool cameraInitialized: false
    property int cameraRetries: 0

    Camera {
        id: qtCamera
        // Prevent auto-start: default cameraState is ActiveState which
        // triggers loading as soon as deviceId is bound, before VideoOutput
        // and other components are ready — causing GStreamer negotiation errors.
        cameraState: Camera.UnloadedState
        deviceId: QtMultimedia.availableCameras.length > 0
                  ? QtMultimedia.availableCameras[cameraSelector.currentIndex].deviceId
                  : ""

        Component.onCompleted: {
            console.log("Camera: initialized, deviceId=" + deviceId
                        + ", state=" + cameraState + ", status=" + cameraStatus)
            window.cameraInitialized = true
            // Start after a short delay so all QML components are fully wired
            cameraStartTimer.start()
        }

        onCameraStatusChanged: {
            if (cameraStatus === Camera.ActiveStatus)
                window.cameraRetries = 0
            var name;
            switch (cameraStatus) {
            case Camera.ActiveStatus:    name = "ActiveStatus";    break;
            case Camera.StartingStatus:  name = "StartingStatus";  break;
            case Camera.LoadedStatus:    name = "LoadedStatus";    break;
            case Camera.LoadingStatus:   name = "LoadingStatus";   break;
            case Camera.UnloadedStatus:  name = "UnloadedStatus";  break;
            case Camera.UnloadingStatus: name = "UnloadingStatus"; break;
            case Camera.StandbyStatus:   name = "StandbyStatus";   break;
            default:                     name = "Unknown";         break;
            }
            console.log("Camera: status changed to " + name + " (" + cameraStatus + ")")
        }

        onErrorChanged: {
            if (errorCode !== Camera.NoError) {
                console.warn("Camera error: " + errorString + " (code " + errorCode + ")")
                if (window.cameraRetries < 1) {
                    window.cameraRetries++
                    cameraRestartTimer.restart()
                }
            }
        }
    }

    // Initial camera start — delayed so all QML components are fully created
    Timer {
        id: cameraStartTimer
        interval: 100
        repeat: false
        onTriggered: {
            console.log("Camera: starting, deviceId=" + qtCamera.deviceId)
            qtCamera.start()
        }
    }

    // Timer to restart camera after device switch (GStreamer needs time to release)
    Timer {
        id: cameraRestartTimer
        interval: 200
        repeat: false
        onTriggered: {
            console.log("Camera: restarting after device switch, deviceId=" + qtCamera.deviceId)
            qtCamera.start()
        }
    }

    QrCodeReader {
        id: qrReader
        target: videoOutput
        source: qtCamera
        scanInterval: 300
        Component.onCompleted: scanning = true

        onQrCodeDetected: {
            console.log("QR Code detected: " + data)
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

    // Horizontal layout: camera on left (2/3), controls on right (1/3)
    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 20

        // Camera area (2/3 of space)
        ColumnLayout {
            Layout.preferredWidth: (parent.width * 2/3) - 20
            Layout.fillHeight: true
            spacing: 10

            // Camera selector
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Label {
                    text: qsTr("Camera:")
                    font.pixelSize: 14
                }

                ComboBox {
                    id: cameraSelector
                    Layout.fillWidth: true
                    model: {
                        var names = [];
                        for (var i = 0; i < QtMultimedia.availableCameras.length; i++) {
                            names.push(QtMultimedia.availableCameras[i].displayName);
                        }
                        return names;
                    }

                    onCurrentIndexChanged: {
                        if (!window.cameraInitialized) {
                            console.log("Camera: ignoring index change during init (index=" + currentIndex + ")")
                            return
                        }
                        console.log("Camera: switching to index " + currentIndex)
                        qtCamera.stop()
                        cameraRestartTimer.restart()
                    }
                }
            }

            // Video output
            VideoOutput {
                id: videoOutput
                source: qtCamera
                Layout.fillWidth: true
                Layout.fillHeight: true
                fillMode: VideoOutput.PreserveAspectFit

                // Camera status overlay - visible when camera is not yet active
                Rectangle {
                    anchors.centerIn: parent
                    width: statusLabel.width + 30
                    height: statusLabel.height + 20
                    radius: 8
                    color: "#cc333333"
                    visible: qtCamera.cameraStatus !== Camera.ActiveStatus

                    Text {
                        id: statusLabel
                        anchors.centerIn: parent
                        color: "white"
                        font.pixelSize: 16
                        text: {
                            switch (qtCamera.cameraStatus) {
                            case Camera.ActiveStatus:   return qsTr("Active")
                            case Camera.StartingStatus: return qsTr("Starting camera...")
                            case Camera.LoadedStatus:   return qsTr("Camera loaded")
                            case Camera.LoadingStatus:  return qsTr("Loading camera...")
                            case Camera.UnloadedStatus: return qsTr("Camera unloaded")
                            case Camera.UnloadingStatus:return qsTr("Unloading camera...")
                            case Camera.StandbyStatus:  return qsTr("Camera on standby")
                            default:                    return qsTr("Camera status: %1").arg(qtCamera.cameraStatus)
                            }
                        }
                    }
                }
            }
        }

        // Control panel on right (1/3 of space)
        Rectangle {
            Layout.preferredWidth: (parent.width / 3) - 10
            Layout.fillHeight: true
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
                        color: qrReader.scanning ? "#d4edda" : "#f8d7da"
                        radius: 5
                        border.color: qrReader.scanning ? "#c3e6cb" : "#f5c6cb"
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
                                    color: qrReader.scanning ? "#28a745" : "#dc3545"
                                }

                                Text {
                                    text: qrReader.scanning ? qsTr("Scanning Active") : qsTr("Scan Stopped")
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: qrReader.scanning ? "#155724" : "#721c24"
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
                        text: qrReader.scanning ? qsTr("■ STOP SCAN") : qsTr("▶ START SCAN")
                        font.pixelSize: 16
                        font.bold: true
                        font.capitalization: Font.AllUppercase

                        background: Rectangle {
                            anchors.fill: parent
                            color: qrReader.scanning ? "#dc3545" : "#28a745"
                            radius: 5
                            border.color: qrReader.scanning ? "#c82333" : "#218838"
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
                            qrReader.scanning = !qrReader.scanning
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
