# Vision QR Code Reader

QR code reading for Qt5 applications using the lightweight **quirc** library. Supports two build variants:

- **Embedded (Vision3)** — captures frames via `grabToImage()` from `HmiDigitalCamera` GStreamer widgets
- **PC (webcam)** — captures raw frames via `QVideoProbe` from Qt's `Camera` element

Both builds share the same `QrCodeReader` C++ class and quirc library; only the QML front-end and `.pro` file differ.

## Architecture Overview

```
Embedded build (main.qml)                 PC build (main_pc.qml)
┌──────────────────────────┐              ┌──────────────────────────┐
│  HmiDigitalCamera        │              │  Camera + VideoOutput    │
│  (GStreamer widget)      │              │  (Qt Camera)             │
└───────────┬──────────────┘              └──────┬──────────┬────────┘
            │                                    │          │
    target  │                            target  │  source  │
            ▼                                    ▼          ▼
┌───────────────────────────────────────────────────────────────────┐
│                       QrCodeReader (C++)                           │
│                                                                   │
│  Path A: grabToImage() timer        Path B: QVideoProbe           │
│  ┌──────────┐  ┌──────────────┐     ┌───────────────────────┐    │
│  │ QTimer   │─▶│ grabToImage()│     │ QVideoProbe           │    │
│  │ (300ms)  │  │ frame grab   │     │ onVideoFrame()        │    │
│  └──────────┘  └──────┬───────┘     │ (throttled by clock)  │    │
│                       │             └───────────┬───────────┘    │
│                       ▼                         ▼                │
│              ┌────────────────────────────────────────┐          │
│              │          decode() → quirc              │          │
│              │  - downscale to QUIRC_MAX_DIM (640)    │          │
│              │  - convert to grayscale                │          │
│              │  - threshold + capstone detection      │          │
│              │  - perspective transform + grid extract│          │
│              │  - Reed-Solomon ECC + payload decode   │          │
│              └────────────────────────────────────────┘          │
│                             │                                    │
│                   debounce (2 consecutive matches)                │
│                             │                                    │
│                   emit qrCodeDetected(data)                      │
└───────────────────────────────────────────────────────────────────┘
                             │
           ┌─────────────────┴──────────────────┐
           ▼                                    ▼
  ┌─────────────────┐                  ┌─────────────────┐
  │   CanHandler    │                  │    QrLogger     │
  │                 │                  │                 │
  │ Raw SocketCAN   │                  │ Daily log files │
  │ LED 1/2 frames  │                  │ YYYY-MM-DD.log  │
  └─────────────────┘                  └─────────────────┘
```

**Path selection is automatic:** if the QML sets `source` (a Camera object), QVideoProbe is used. If only `target` (a QQuickItem) is set, the `grabToImage()` timer path is used.

## Components

### QrCodeReader

Core QR decoding class, shared by both builds. Wraps the quirc library and exposes a QML type.

### CanHandler

Sends CAN bus frames in response to QR code detections. Uses raw **SocketCAN** (`PF_CAN / SOCK_RAW`) — no dependency on the Qt QCanBus module.

**CAN IDs (extended 29-bit frames):**

| ID           | Description              |
|--------------|--------------------------|
| `0x18FF0000` | LED 1 control            |
| `0x18FF0001` | LED 2 control            |
| `0x18FF00FF` | Cyclic debug frame (1 s) |

**QR code command format** — each line of the QR code payload is parsed independently:

```
LED 1 : ON
LED 1 : OFF
LED 2 : ON
LED 2 : OFF
```

A single QR code can contain multiple commands (one per line). Unrecognized lines are silently ignored.

**Frame encoding:**

- `state = true` → `data[0..7] = 0xFF`
- `state = false` → `data[0] = 0xFE`, rest `0xFF`
- Debug frame → `data[0..7] = 0xAA`

The HAL initializes CAN at startup (`hal_can_init`, `hal_can_init_channel` at 250 kbps); `CanHandler` then opens a raw socket on the resulting interface (default: `can0`).

### QrLogger

Logs every QR scan to a daily text file under `/data/vision_qr_code/logs/`.

**Log file naming:** `YYYY-MM-DD.log`

**Entry format:**

```
=== QR Code log — DD/MM/YYYY ===
[hh:mm:ss] [VALIDE]   LED 1 : ON | LED 2 : OFF
[hh:mm:ss] [INVALIDE] <raw QR content>
```

- A blank line is inserted when the entry type changes (valid ↔ invalid).
- The logger detects midnight automatically: if the date changes between two scans, a new file is opened.
- If the file for the current date already exists, entries are appended; otherwise a new file is created with a header.

### UsbWatcher

Detects USB mass storage insertion/removal via the Vision 3 HAL (`hal_blockdevice` / `hal_udev`).

- Exposes a `usbPresent` QML property for UI binding.
- `exportLogs(logDir)` copies all files from `logDir` to `<USB mount>/Logs/`, then deletes them from internal storage.
- USB drives inserted before application start are detected immediately (HAL replay mode).

**Signals:**

| Signal | Parameters | Description |
|--------|------------|-------------|
| `usbPresentChanged` | — | USB inserted or removed |
| `exportResult` | bool success, QString message | Export operation completed |

### DateTimeHandler

Sets the system clock and RTC via `Q_INVOKABLE bool setDateTime(day, month, year, hour, minute)`. Exposed as a QML singleton (`com.datetime 1.0`).

## QML User Interface (Embedded)

### Layout

```
┌─────────────────────────────────────────────────────┐
│                                                     │
│   HmiDigitalCamera (2/3)  │  Control panel (1/3)   │
│                           │  - Scan status          │
│                           │  - START/STOP button    │
│                           │  - Last QR result (5 s) │
│                                                     │
├─────────────────────────────────────────────────────┤
│ [✎ date heure]                         [USB icon]  │
└─────────────────────────────────────────────────────┘
```

### Scan behaviour

1. **Valid QR code** (`LED 1/2 : ON/OFF`): result displayed in the right panel for 5 seconds, then cleared automatically.
2. **Invalid QR code** (unrecognized content): scanning is paused and a red overlay covers the left panel. The operator must press **Recommencer** to resume.

### Date/time popup

Tapping the pencil icon (bottom-left) opens a popup with +/− spinners for day, month, year, hour, minute. Confirming calls `DateTimeHandler.setDateTime()`.

### USB export popup

The USB icon (bottom-right) is visible only when `usbWatcher.usbPresent` is `true`. Tapping it opens a popup with an **Exporter les logs** button that calls `usbWatcher.exportLogs()`.

## QML Integration

### Registering Types (main.cpp)

```cpp
qmlRegisterType<QrCodeReader>("com.qrcode", 1, 0, "QrCodeReader");
qmlRegisterSingletonType<DateTimeHandler>("com.datetime", 1, 0, "DateTimeHandler", ...);
engine.rootContext()->setContextProperty("usbWatcher", &usbWatcher);
```

### Embedded Usage (main.qml)

```qml
import com.qrcode 1.0

QrCodeReader {
    id: qrReader1
    target: camera1         // HmiDigitalCamera to grab frames from
    scanInterval: 300
    Component.onCompleted: scanning = true

    onQrCodeDetected: {
        // data may contain multiple "LED x : ON/OFF" lines
    }
}
```

### PC Webcam Usage (main_pc.qml)

```qml
import com.qrcode 1.0

QrCodeReader {
    id: qrReader
    target: videoOutput     // VideoOutput (fallback if probe fails)
    source: qtCamera        // Camera — enables QVideoProbe path
    scanInterval: 300
    Component.onCompleted: scanning = true
}
```

### QrCodeReader Properties

| Property | Type | Description |
|----------|------|-------------|
| `target` | QQuickItem* | QML item to capture frames from (via `grabToImage()`) |
| `source` | QObject* | Camera QML element for QVideoProbe raw frame capture |
| `scanning` | bool | Enable/disable continuous scanning |
| `scanInterval` | int | Milliseconds between scans (minimum: 100ms) |
| `lastResult` | QString | Most recently detected QR code content |

### QrCodeReader Signals

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

## The quirc Library

**quirc** is a lightweight QR code recognition library chosen for embedded use:

| Property | Value |
|----------|-------|
| Language | Pure C |
| Dependencies | None |
| Binary size | ~50 KB |
| Performance | ~50 ms per VGA frame |
| License | ISC (permissive) |
| Source | [dlbeer/quirc](https://github.com/dlbeer/quirc) |

## File Structure

```
src/
├── main.cpp              # Embedded entry point — HAL init, CAN init, QrLogger, UsbWatcher
├── main.qml              # Embedded UI (HmiDigitalCamera, control panel, popups)
├── qml.qrc               # Embedded Qt resources
├── vision_qr_code.pro    # Embedded qmake project (GStreamer, libhal)
├── main_pc.cpp           # PC entry point
├── main_pc.qml           # PC UI (Camera + VideoOutput + QR reader)
├── qml_pc.qrc            # PC Qt resources
├── vision_qr_code_pc.pro # PC qmake project
├── QrCodeReader.h/.cpp   # Qt quirc wrapper (shared)
├── CanHandler.h/.cpp     # Raw SocketCAN LED control
├── QrLogger.h/.cpp       # Daily scan log writer
├── UsbWatcher.h/.cpp     # HAL USB detection + log export
├── DateTimeHandler.h/.cpp# RTC / system clock setter
└── quirc/                # QR decoding library (shared)
    ├── quirc.h
    ├── quirc_internal.h
    ├── quirc.c
    ├── identify.c
    ├── decode.c
    └── version_db.c
```

## Building

### Embedded (Vision3 target)

```bash
# From Qt Creator: select Vision3 kit, Build → Rebuild All
# Or from the command line:
qmake vision_qr_code.pro
make
```

Deploys to `/data/vision_qr_code/bin/` on the target. Logs are written to `/data/vision_qr_code/logs/`.

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

## Frame Capture Paths

### Path A: `grabToImage()` (embedded)

Used when `target` is set but `source` is not. A QTimer fires every `scanInterval` ms and grabs the rendered content of the target QQuickItem.

```
QTimer fires → performScan()
                  │
                  ├─ skip if previous grab still in-flight
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

## Performance Considerations

- **QUIRC_MAX_DIM (640)**: Images larger than 640 px in either dimension are downscaled before quirc processing, preventing integer overflow in `line_intersect()` and limiting CPU usage per frame
- **Frame capture**: `grabToImage()` is GPU-accelerated on Vision3; QVideoProbe provides zero-copy Y-channel extraction on PC
- **Scan interval**: 300 ms default (~3 scans/sec) balances responsiveness with CPU usage
- **Image size**: Embedded cameras render at 320×240; PC webcams are downscaled to QUIRC_MAX_DIM
- **Memory**: quirc allocates ~300 KB for VGA resolution processing

## Limitations

1. **Embedded frame capture**: Uses `grabToImage()` which captures the rendered output, not raw camera frames. Any overlays or transformations applied to the camera view will be included in the captured image. (The PC build uses `QVideoProbe` for raw frames, avoiding this issue.)

2. **Single QR per frame**: Returns only the first successfully decoded QR code if multiple are present.

3. **Lighting conditions**: Performance depends on camera exposure and QR code contrast.
