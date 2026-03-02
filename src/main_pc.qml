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
    title: qsTr("Vision QR Code - PC Webcam POC")

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
            qrResultRect.visible = true
            qrHideTimer.restart()
        }
    }

    Timer {
        id: qrHideTimer
        interval: 5000
        onTriggered: qrResultRect.visible = false
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
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

        // Controls and results
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                id: qrScanToggle
                text: qrReader.scanning ? qsTr("Stop QR Scan") : qsTr("Start QR Scan")
                font.pixelSize: 14

                onClicked: {
                    qrReader.scanning = !qrReader.scanning
                }
            }

            // Scanning indicator
            Rectangle {
                width: 140
                height: 30
                color: qrReader.scanning ? "#aaffaa" : "#ffaaaa"
                radius: 3

                Text {
                    anchors.centerIn: parent
                    text: qrReader.scanning ? qsTr("Scanning...") : qsTr("Scan Stopped")
                    font.pixelSize: 12
                }
            }

            // QR Code result display
            Rectangle {
                id: qrResultRect
                Layout.fillWidth: true
                height: 60
                color: "#e0ffe0"
                border.color: "#00aa00"
                border.width: 2
                radius: 5
                visible: false

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 5

                    Text {
                        text: qsTr("QR:")
                        font.pixelSize: 12
                        font.bold: true
                        color: "#006600"
                    }

                    Text {
                        id: qrResultText
                        Layout.fillWidth: true
                        text: ""
                        font.pixelSize: 12
                        wrapMode: Text.WrapAnywhere
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        color: "#000000"
                    }
                }
            }
        }
    }
}
