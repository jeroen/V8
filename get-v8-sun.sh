LIBDIR="$PWD/.deps"
url="http://jeroen.github.io/V8/v8-7.0.276.38-sun32.tar.gz"
mkdir -p $LIBDIR
curl -sSL $url -o libs.tar.gz
if [ -f "libs.tar.gz" ]; then
  gtar zxf libs.tar.gz -C $LIBDIR
  rm -f libs.tar
  V8PATH="$LIBDIR/v8-7.0.276.38-sun32"
  if [ -f "${V8PATH}/lib/libv8_base.a" ]; then
    PKG_CFLAGS="-I${V8PATH}/include -DV8_ENABLE_CHECKS"
    PKG_LIBS="-L${V8PATH}/lib -lv8_base -lv8_libplatform  -lv8_snapshot -lv8_libbase -lv8_libsampler -lv8_init -lv8_initializers"
    sed -e "s|@cflags@|$PKG_CFLAGS|" -e "s|@libs@|$PKG_LIBS|" src/Makevars.in > src/Makevars
    exit 0
  fi
fi
