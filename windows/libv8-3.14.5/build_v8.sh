# Run this script in msys1 (not msys2)
# Edit /etc/fstab to point to the proper compiler (x64 or i386)
# We use python.exe from msys2 because windows python generates paths with backslashes.
rm -Rf v8-3.14.5
tar xzvf v8-3.14.5.tar.gz
cd v8-3.14.5
PYTHON=/c/msys32/mingw64/bin/python
TARGET="x64"

make dependencies
GYP_GENERATORS=make \
$PYTHON build/gyp_v8 \
  -Dv8_enable_i18n_support=true \
  -Duse_system_icu=1 \
  -Dconsole=readline \
  -Dcomponent=static_library \
  -Dv8_target_arch="$TARGET" \
  --generator-output=out \
  -f make

LINK=g++ make -C out BUILDTYPE=Release mksnapshot V=1 || true
LINK=g++ make -C out BUILDTYPE=Release V=1 || true
