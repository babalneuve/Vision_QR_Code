QT += quick qml multimedia widgets serialbus
CONFIG += c++11

DEFINES += QT_DEPRECATED_WARNINGS

TARGET = vision_qr_code_pc

SOURCES += \
        main_pc.cpp \
        QrCodeReader.cpp \
        CanHandler.cpp \
        quirc/quirc.c \
        quirc/decode.c \
        quirc/identify.c \
        quirc/version_db.c

HEADERS += \
        QrCodeReader.h \
        CanHandler.h \
        quirc/quirc.h \
        quirc/quirc_internal.h

RESOURCES += qml_pc.qrc translations.qrc

TRANSLATIONS += translations/fr.ts

# Compile .ts to .qm before rcc processes translations.qrc
isEmpty(QMAKE_LRELEASE):QMAKE_LRELEASE = $$dirname(QMAKE_QMAKE)/lrelease

updateqm.input = TRANSLATIONS
updateqm.output = ${QMAKE_FILE_PATH}/${QMAKE_FILE_BASE}.qm
updateqm.commands = $$QMAKE_LRELEASE ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_OUT}
updateqm.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += updateqm

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
