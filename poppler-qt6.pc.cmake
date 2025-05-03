prefix=${pcfiledir}/../..
libdir=${prefix}/lib
includedir=${prefix}/include

Name: poppler-qt6
Description: Qt6 bindings for poppler
Version: @POPPLER_VERSION@
Requires: @PC_REQUIRES@
@PC_REQUIRES_PRIVATE@

Libs: -L${libdir} -lpoppler-qt6
Cflags: -I${includedir}/poppler/qt6
