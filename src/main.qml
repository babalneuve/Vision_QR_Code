import QtQuick 2.9
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.11
import QtQuick.Window 2.12
import QtMultimedia 5.11
/*import QtQuick.VirtualKeyboard 2.1*/
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

    // QR Code Reader for camera1
    QrCodeReader {
        id: qrReader1
        target: camera1
        scanInterval: 300
        scanning: true

        onQrCodeDetected: {
            console.log("QR Code detected on Camera 1: " + data)
            qrResultText.text = data
            qrResultRect.visible = true
            qrHideTimer.restart()
        }
    }

    // QR Code Reader for camera2
    QrCodeReader {
        id: qrReader2
        target: camera2
        scanInterval: 300
        scanning: true

        onQrCodeDetected: {
            console.log("QR Code detected on Camera 2: " + data)
            qrResultText.text = data
            qrResultRect.visible = true
            qrHideTimer.restart()
        }
    }

    // Timer to hide QR result after a delay
    Timer {
        id: qrHideTimer
        interval: 5000
        onTriggered: qrResultRect.visible = false
    }

    Item {
        id: itemId
        objectName: "itemId"
        width: 1280
        height: 800

        transform: Scale { origin.x: 0; origin.y: 0; xScale: displayScaleX; yScale: displayScaleY}

        /* Simple layout to put multiple items next to each other */
        RowLayout {
            id: rowLayout
            anchors.fill: parent
            spacing: 10

            HmiDigitalCamera {
                id: camera1
                objectName: "camera1"
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                width: 320
                height: 240
                camera_info: {
                    "ip": "10.100.30.3",
                    "port": 50003,
                    "path": "/data/camera1_%d.mp4",
                    "recording_filename": "camera1"
                }
            }

            HmiDigitalCamera {
                id: camera2
                objectName: "camera2"
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                width: 320
                height: 240
                camera_info: {
                    "ip": "10.100.30.32",
                    "port": 50004,
                    "path": "/data/camera2_%d.mp4",
                    "recording_filename": "camera2"
                }

                /* Part of the frame can be cut out by setting positive values
                 * in the crop properties */
                crop_top: 0
                crop_left: 0
                crop_right: 0
                crop_bottom: 0

                /* Disable/enable the aspect ratio (by default 'force_aspect_ratio' is 'true')*/
                force_aspect_ratio: true
            }

            ColumnLayout {
                spacing: 10
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                TextInput {
                    id: ipcamera1
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 40
                    text: camera1.camera_info.ip + ":" + camera1.camera_info.port.toString()
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
/*                  onActiveFocusChanged: {
                        if (activeFocus) {
                            Qt.inputMethod.show();
                        }
                    } */
                }

                Button {
                    id: changeIPButton1
                    objectName: "ip_button1"
                    Layout.preferredWidth: 170
                    Layout.preferredHeight: 40
                    text: qsTr("Change Camera1 IP")
                    font.pixelSize: 15

                    signal ipChange1(ip: string)

                    onClicked: changeIPButton1.ipChange1(ipcamera1.text);
                }

                TextInput {
                    id: ipcamera2
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 40
                    text: camera2.camera_info.ip + ":" + camera2.camera_info.port.toString()
/*                  inputMethodHints: Qt.ImhFormattedNumbersOnly
                    onClicked: {
                        Qt.inputMethod.show();
                    } */
                }

                Button {
                    id: changeIPButton2
                    objectName: "ip_button2"
                    Layout.preferredWidth: 170
                    Layout.preferredHeight: 40
                    text: qsTr("Change Camera2 IP")
                    font.pixelSize: 15

                    signal ipChange2(ip: string)

                    onClicked: changeIPButton2.ipChange2(ipcamera2.text);
                }
                
        Button {
                    id: resetPipelineButton1
                    objectName: "reset_pipeline_button1"
                    Layout.preferredWidth: 170
                    Layout.preferredHeight: 40
                    text: qsTr("Reset pipeline 1")
                    font.pixelSize: 15

                    signal resetPipeline1()

                    onClicked: {
                        camera1.visible = false;
                        resetPipelineButton1.resetPipeline1();
                        camera1.visible = true;
                    }
                }
        
        Button {
                    id: stopButton1
                    objectName: "stop_button1"
                    Layout.preferredWidth: 170
                    Layout.preferredHeight: 40
                    text: qsTr("Stop pipeline 1")
                    font.pixelSize: 15

                    signal stopPipeline1()

                    onClicked: {
                        stopButton1.stopPipeline1();
                    }
                }
        
        Button {
                    id: resetPipelineButton2
                    objectName: "reset_pipeline_button2"
                    Layout.preferredWidth: 170
                    Layout.preferredHeight: 40
                    text: qsTr("Reset pipeline 2")
                    font.pixelSize: 15

                    signal resetPipeline2()

                    onClicked: {
                        camera2.visible = false;
                        resetPipelineButton2.resetPipeline2();
                        camera2.visible = true;
                    }
                 }
        
        Button {
                    id: stopButton2
                    objectName: "stop_button2"
                    Layout.preferredWidth: 170
                    Layout.preferredHeight: 40
                    text: qsTr("Stop pipeline 2")
                    font.pixelSize: 15

                    signal stopPipeline2()

                    onClicked: {
                        stopButton2.stopPipeline2();
                    }
                }

                // QR Scanning toggle button
                Button {
                    id: qrScanToggle
                    Layout.preferredWidth: 170
                    Layout.preferredHeight: 40
                    text: qrReader1.scanning ? qsTr("Stop QR Scan") : qsTr("Start QR Scan")
                    font.pixelSize: 15

                    onClicked: {
                        qrReader1.scanning = !qrReader1.scanning
                        qrReader2.scanning = !qrReader2.scanning
                    }
                }

                // QR Code result display
                Rectangle {
                    id: qrResultRect
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 80
                    color: "#e0ffe0"
                    border.color: "#00aa00"
                    border.width: 2
                    radius: 5
                    visible: false

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 5

                        Text {
                            text: qsTr("QR Code Detected:")
                            font.pixelSize: 12
                            font.bold: true
                            color: "#006600"
                        }

                        Text {
                            id: qrResultText
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: ""
                            font.pixelSize: 11
                            wrapMode: Text.WrapAnywhere
                            elide: Text.ElideRight
                            maximumLineCount: 3
                            color: "#000000"
                        }
                    }
                }

                // Scanning indicator
                Rectangle {
                    Layout.preferredWidth: 170
                    Layout.preferredHeight: 30
                    color: qrReader1.scanning ? "#aaffaa" : "#ffaaaa"
                    radius: 3

                    Text {
                        anchors.centerIn: parent
                        text: qrReader1.scanning ? qsTr("Scanning...") : qsTr("Scan Stopped")
                        font.pixelSize: 12
                    }
                }
            }
        }
    }

/*    InputPanel {
        id: inputPanel
        z: 100
        anchors.fill: parent
        visible: Qt.inputMethod.visible
    } */
}
