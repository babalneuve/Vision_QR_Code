# Vision QR Code Reader

QR code reading capability for Vision3 embedded Qt5 applications using the lightweight **quirc** library.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         main.qml                                │
│  ┌─────────────────┐  ┌─────────────────┐  ┌────────────────┐  │
│  │ HmiDigitalCamera│  │ HmiDigitalCamera│  │   UI Controls  │  │
│  │    (camera1)    │  │    (camera2)    │  │ - Scan toggle  │  │
│  └────────┬────────┘  └────────┬────────┘  │ - Result box   │  │
│           │                    │           │ - Status LED   │  │
│           ▼                    ▼           └────────────────┘  │
│  ┌─────────────────┐  ┌─────────────────┐                      │
│  │  QrCodeReader   │  │  QrCodeReader   │                      │
│  │   (qrReader1)   │  │   (qrReader2)   │                      │
│  └────────┬────────┘  └────────┬────────┘                      │
└───────────┼────────────────────┼────────────────────────────────┘
            │                    │
            ▼                    ▼
┌─────────────────────────────────────────────────────────────────┐
│                    QrCodeReader (C++)                           │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────────────┐ │
│  │  QTimer      │  │ grabToImage()│  │   quirc library       │ │
│  │  (300ms)     │──▶│ frame grab  │──▶│ - threshold          │ │
│  └──────────────┘  └──────────────┘  │ - find capstones      │ │
│                                       │ - extract grid        │ │
│                                       │ - decode payload      │ │
│                                       └───────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

## How Frame Capture Works

The `HmiDigitalCamera` is a proprietary Vision3 SDK component that renders video via GStreamer. Since it doesn't expose raw frame data directly, we use Qt's **`QQuickItem::grabToImage()`** method to capture rendered frames.

### Frame Capture Flow

```
1. QTimer fires every 300ms
         │
         ▼
2. QrCodeReader::performScan()
         │
         ▼
3. m_target->grabToImage()
   - Captures the QQuickItem's rendered content
   - Returns QSharedPointer<QQuickItemGrabResult>
         │
         ▼
4. QQuickItemGrabResult::ready signal
         │
         ▼
5. QrCodeReader::onGrabComplete()
   - Retrieves QImage from grab result
   - Calls decode(image)
         │
         ▼
6. Convert to grayscale (Format_Grayscale8)
         │
         ▼
7. Copy pixels to quirc buffer
         │
         ▼
8. quirc_end() - Process image
         │
         ▼
9. quirc_extract() + quirc_decode()
         │
         ▼
10. Emit qrCodeDetected(QString) signal
```

### Key Code (QrCodeReader.cpp)

```cpp
void QrCodeReader::performScan()
{
    // Capture frame from QML item
    QSharedPointer<QQuickItemGrabResult> result = m_target->grabToImage();
    if (result) {
        connect(result.data(), &QQuickItemGrabResult::ready,
                this, &QrCodeReader::onGrabComplete);
    }
}

void QrCodeReader::onGrabComplete()
{
    QQuickItemGrabResult *result = qobject_cast<QQuickItemGrabResult*>(sender());
    QImage image = result->image();
    QString decoded = decode(image);

    if (!decoded.isEmpty() && decoded != m_lastResult) {
        m_lastResult = decoded;
        emit qrCodeDetected(decoded);
    }
}
```

## The quirc Library

**quirc** is a lightweight QR code recognition library chosen for embedded use:

| Property | Value |
|----------|-------|
| Language | Pure C |
| Dependencies | None |
| Binary size | ~50KB |
| Performance | ~50ms per VGA frame |
| License | ISC (permissive) |

### Processing Pipeline

```
Input Image (grayscale)
         │
         ▼
┌─────────────────────┐
│  Adaptive Threshold │  Convert to binary black/white
└─────────┬───────────┘
         │
         ▼
┌─────────────────────┐
│   Region Analysis   │  Flood-fill to find connected regions
└─────────┬───────────┘
         │
         ▼
┌─────────────────────┐
│  Capstone Detection │  Find the 3 corner squares (1:1:3:1:1 ratio)
└─────────┬───────────┘
         │
         ▼
┌─────────────────────┐
│   Grid Extraction   │  Perspective transform to extract cell grid
└─────────┬───────────┘
         │
         ▼
┌─────────────────────┐
│   Reed-Solomon ECC  │  Error correction
└─────────┬───────────┘
         │
         ▼
┌─────────────────────┐
│   Payload Decode    │  Numeric/Alphanumeric/Byte/Kanji modes
└─────────────────────┘
```

## QML Integration

### Registering the Type (main.cpp)

```cpp
qmlRegisterType<QrCodeReader>("com.qrcode", 1, 0, "QrCodeReader");
```

### Using in QML (main.qml)

```qml
import com.qrcode 1.0

QrCodeReader {
    id: qrReader1
    target: camera1          // The HmiDigitalCamera to scan
    scanInterval: 300        // Scan every 300ms
    scanning: true           // Start scanning immediately

    onQrCodeDetected: {
        console.log("QR Code: " + data)
        resultText.text = data
    }
}
```

### Available Properties

| Property | Type | Description |
|----------|------|-------------|
| `target` | QQuickItem* | The camera component to capture frames from |
| `scanning` | bool | Enable/disable continuous scanning |
| `scanInterval` | int | Milliseconds between scans (min: 100ms) |
| `lastResult` | QString | Most recently detected QR code content |

### Signals

| Signal | Parameters | Description |
|--------|------------|-------------|
| `qrCodeDetected` | QString data | Emitted when a new QR code is detected |
| `scanComplete` | bool found, int timeMs | Emitted after each scan attempt |

## Debouncing

To prevent duplicate detections, the reader implements debouncing:

1. Same QR code must be detected **2 consecutive times** before emitting signal
2. Signal only emits if the result **differs from the last reported** result
3. When no QR code is visible, debounce state resets

## File Structure

```
src/
├── main.cpp              # App entry, registers QrCodeReader
├── main.qml              # UI with cameras and QR readers
├── QrCodeReader.h        # Qt wrapper class header
├── QrCodeReader.cpp      # Qt wrapper implementation
├── qml.qrc               # Qt resources
├── vision_qr_code.pro    # qmake project file
└── quirc/                # QR decoding library
    ├── quirc.h           # Public API
    ├── quirc_internal.h  # Internal structures
    ├── quirc.c           # Core functions
    ├── identify.c        # QR code detection
    ├── decode.c          # Payload decoding
    └── version_db.c      # QR version specifications
```

## Building

```bash
# Using qmake
qmake vision_qr_code.pro
make

# Or open in Qt Creator and build for Vision3 target
```

## Performance Considerations

- **Frame capture**: `grabToImage()` is GPU-accelerated on Vision3
- **Scan interval**: 300ms provides good responsiveness without CPU overload
- **Image size**: Camera renders at 320x240, which is efficient for QR detection
- **Memory**: quirc allocates ~300KB for VGA resolution processing

## Limitations

1. **Frame capture method**: Uses `grabToImage()` which captures the rendered output, not raw camera frames. This means any overlays or transformations applied to the camera view will be included.

2. **Single QR per frame**: Currently returns only the first successfully decoded QR code if multiple are present.

3. **Lighting conditions**: Performance depends on camera exposure and QR code contrast.
