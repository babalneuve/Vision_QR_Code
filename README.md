# Vision QR Code Reader

QR code reading for Qt5 applications using the lightweight **quirc** library. Supports two build variants:

- **Embedded (Vision3)** — captures frames via `grabToImage()` from `HmiDigitalCamera` GStreamer widgets
- **PC (webcam)** — captures raw frames via `QVideoProbe` from Qt's `Camera` element

Both builds share the same `QrCodeReader` C++ class and quirc library; only the QML front-end and `.pro` file differ.

## Architecture Overview

```
Embedded build (main.qml)              PC build (main_pc.qml)
┌──────────────────────────┐           ┌──────────────────────────┐
│  HmiDigitalCamera        │           │  Camera + VideoOutput    │
│  (GStreamer widget)      │           │  (Qt Camera)             │
└───────────┬──────────────┘           └──────┬──────────┬────────┘
            │                                 │          │
    target  │                         target  │  source  │
            ▼                                 ▼          ▼
┌──────────────────────────────────────────────────────────────────┐
│                    QrCodeReader (C++)                             │
│                                                                  │
│  Path A: grabToImage() timer         Path B: QVideoProbe         │
│  ┌──────────┐  ┌──────────────┐      ┌───────────────────────┐   │
│  │ QTimer   │─▶│ grabToImage()│      │ QVideoProbe           │   │
│  │ (300ms)  │  │ frame grab   │      │ onVideoFrame()        │   │
│  └──────────┘  └──────┬───────┘      │ (throttled by clock)  │   │
│                       │              └───────────┬───────────┘   │
│                       ▼                          ▼               │
│              ┌─────────────────────────────────────────┐         │
│              │          decode() → quirc               │         │
│              │  - downscale to QUIRC_MAX_DIM (640)     │         │
│              │  - convert to grayscale                 │         │
│              │  - threshold + capstone detection       │         │
│              │  - perspective transform + grid extract │         │
│              │  - Reed-Solomon ECC + payload decode    │         │
│              └─────────────────────────────────────────┘         │
│                              │                                   │
│                    debounce (2 consecutive matches)               │
│                              │                                   │
│                    emit qrCodeDetected(data)                     │
└──────────────────────────────────────────────────────────────────┘
```

**Path selection is automatic:** if the QML sets `source` (a Camera object), QVideoProbe is used. If only `target` (a QQuickItem) is set, the `grabToImage()` timer path is used.

## Frame Capture Paths

### Path A: `grabToImage()` (embedded)

Used when `target` is set but `source` is not. A QTimer fires every `scanInterval` ms and grabs the rendered content of the target QQuickItem.

```
QTimer fires → performScan()
                  │
                  ├─ skip if previous grab still in-flight
                  │
                  ▼
              m_target->grabToImage()
                  │
                  ▼  (async)
              onGrabComplete()
                  │
                  ▼
              decode(image) → debounce → emit
```

### Path B: `QVideoProbe` (PC webcam)

Used when `source` is set to a Camera QML element. The probe fires on every camera frame; processing is throttled by `m_frameClock` to respect `scanInterval`.

```
QVideoProbe::videoFrameProbed → onVideoFrame()
                                    │
                                    ├─ skip if elapsed < scanInterval
                                    │
                                    ▼
                              map QVideoFrame to CPU
                                    │
                                    ▼
                              extract Y channel (YUV formats)
                              or convert (RGB formats)
                                    │
                                    ▼
                              decode(image) → debounce → emit
```

Supported pixel formats: YUYV, UYVY, NV12, NV21, YUV420P, YV12, RGB32, ARGB32, BGR32, plus any format Qt can map via `imageFormatFromPixelFormat()`.

## The quirc Library

**quirc** is a lightweight QR code recognition library chosen for embedded use:

| Property | Value |
|----------|-------|
| Language | Pure C |
| Dependencies | None |
| Binary size | ~50KB |
| Performance | ~50ms per VGA frame |
| License | ISC (permissive) |
| Source | Upstream from [dlbeer/quirc](https://github.com/dlbeer/quirc) |

## QML Integration

### Registering the Type (main.cpp / main_pc.cpp)

```cpp
qmlRegisterType<QrCodeReader>("com.qrcode", 1, 0, "QrCodeReader");
```

### Embedded Usage (main.qml)

```qml
import com.qrcode 1.0

QrCodeReader {
    id: qrReader1
    target: camera1          // HmiDigitalCamera to grab frames from
    scanInterval: 300
    Component.onCompleted: scanning = true

    onQrCodeDetected: {
        console.log("QR Code: " + data)
        resultText.text = data
    }
}
```

### PC Webcam Usage (main_pc.qml)

```qml
import com.qrcode 1.0

QrCodeReader {
    id: qrReader
    target: videoOutput      // VideoOutput (fallback if probe fails)
    source: qtCamera         // Camera — enables QVideoProbe path
    scanInterval: 300
    Component.onCompleted: scanning = true

    onQrCodeDetected: {
        console.log("QR Code: " + data)
        resultText.text = data
    }
}
```

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `target` | QQuickItem* | QML item to capture frames from (via `grabToImage()`) |
| `source` | QObject* | Camera QML element for QVideoProbe raw frame capture |
| `scanning` | bool | Enable/disable continuous scanning |
| `scanInterval` | int | Milliseconds between scans (minimum: 100ms) |
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
4. When scanning is **stopped**, both debounce state and `lastResult` are cleared — so restarting scanning can re-detect the same QR code

## File Structure

```
src/
├── main.cpp              # Embedded entry point, registers QrCodeReader
├── main.qml              # Embedded UI (HmiDigitalCamera + QR readers)
├── qml.qrc               # Embedded Qt resources
├── vision_qr_code.pro    # Embedded qmake project (links gstreamer-1.0)
├── main_pc.cpp           # PC entry point, registers QrCodeReader
├── main_pc.qml           # PC UI (Camera + VideoOutput + QR reader)
├── qml_pc.qrc            # PC Qt resources
├── vision_qr_code_pc.pro # PC qmake project
├── QrCodeReader.h        # Qt wrapper class header (shared)
├── QrCodeReader.cpp      # Qt wrapper implementation (shared)
└── quirc/                # QR decoding library (shared)
    ├── quirc.h           # Public API
    ├── quirc_internal.h  # Internal structures
    ├── quirc.c           # Core functions
    ├── identify.c        # QR code detection
    ├── decode.c          # Payload decoding
    └── version_db.c      # QR version specifications
```

## Building

### Embedded (Vision3 target)

```bash
# From Qt Creator: select Vision3 kit, Build → Rebuild All
# Or from command line:
qmake vision_qr_code.pro
make
```

Deploys to `/data/vision_qr_code/bin/` on the target.

### PC (desktop webcam)

```bash
mkdir -p build-pc && cd build-pc
qmake ../src/vision_qr_code_pc.pro
make
./vision_qr_code_pc
```

### Clean Rebuild

After updating quirc sources, a clean rebuild is required to avoid stale object files:

```bash
cd <build-dir>
make clean
qmake ../src/<project>.pro
make
```

## Performance Considerations

- **QUIRC_MAX_DIM (640)**: Images larger than 640px in either dimension are downscaled before quirc processing, preventing integer overflow in `line_intersect()` and limiting CPU usage per frame
- **Frame capture**: `grabToImage()` is GPU-accelerated on Vision3; QVideoProbe provides zero-copy Y-channel extraction on PC
- **Scan interval**: 300ms default (~3 scans/sec) balances responsiveness with CPU usage
- **Image size**: Embedded cameras render at 320x240; PC webcams are downscaled to QUIRC_MAX_DIM
- **Memory**: quirc allocates ~300KB for VGA resolution processing

## Limitations

1. **Embedded frame capture**: Uses `grabToImage()` which captures the rendered output, not raw camera frames. Any overlays or transformations applied to the camera view will be included in the captured image. (The PC build uses `QVideoProbe` for raw frames, avoiding this issue.)

2. **Single QR per frame**: Returns only the first successfully decoded QR code if multiple are present.

3. **Lighting conditions**: Performance depends on camera exposure and QR code contrast.
