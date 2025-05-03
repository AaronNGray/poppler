prefix=${pcfiledir}/../..
libdir=${prefix}/lib
includedir=${prefix}/include

Name: poppler
Description: PDF rendering library
Version: @POPPLER_VERSION@

Libs: -L${libdir} -lpoppler
Cflags: -I${includedir}/poppler
