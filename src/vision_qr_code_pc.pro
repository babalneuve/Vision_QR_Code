QT += quick qml multimedia widgets
CONFIG += c++11

DEFINES += QT_DEPRECATED_WARNINGS

TARGET = vision_qr_code_pc

SOURCES += \
        main_pc.cpp \
        QrCodeReader.cpp \
        quirc/quirc.c \
        quirc/decode.c \
        quirc/identify.c \
        quirc/version_db.c

HEADERS += \
        QrCodeReader.h \
        quirc/quirc.h \
        quirc/quirc_internal.h

RESOURCES += qml_pc.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
