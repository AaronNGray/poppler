prefix=${pcfiledir}/../..
libdir=${prefix}/lib
includedir=${prefix}/include

Name: poppler-cpp
Description: cpp backend for Poppler PDF rendering library
Version: @POPPLER_VERSION@
Requires: @PC_REQUIRES@
@PC_REQUIRES_PRIVATE@

Libs: -L${libdir} -lpoppler-cpp
Cflags: -I${includedir}/poppler/cpp
