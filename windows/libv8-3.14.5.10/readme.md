Notes from Jeroen
-----------------

- We need libv8 around version 3.14. This is the version node
  currently uses. More recent versions of v8 don't work.
- The libv8 build does not play nice with Rtools multilib (-m64).
- MSYS2 tool chain did not work either because it uses seh0/dwarf
  exceptions but Rtools (and hence R) currently uses sj0 only.
- GYP needs a mingw build of python to work. Python for windows
  generates paths with backslashes which messes up everything.
  We can either use one from msys2 or included with mingw-builds.
- SVN is required for GYP. The windows version (Turtoise) is fine.

Update (Aug 2015): rebuild of v8 version 3.14.5.10:

- lib/i386-old was built with Rtools gcc 4.6.3-pre
- lib/i386 was built with i686-4.9.2-release-win32-sjlj
- lib/x64 was built with x86_64-4.9.2-release-win32-sjlj

Required Tools
--------------

- msys 1.0
- mingw-w64 build for x64 with sj0 (via Mingw-builds installer)
- mingw-w64 build for i386 with sj0 (via Mingw-builds installer)
- turtoise
- Python for mingw


Building libs
-------------

- Edit C:\msys\1.0\etc\fstab to point to mount compiler e.g:

   c:/mingw-w64/x86_64-4.9.2-posix-sjlj-rt_v3-rev0/mingw64 /mingw

- Start MSYS shell.
- Test that we have the right compiler (gcc --version)
- Download https://github.com/v8/v8-git-mirror/archive/3.14.5.10.tar.gz
- Run the build script:


```
#Source: wget https://github.com/v8/v8-git-mirror/archive/3.14.5.10.tar.gz

# Add python to path
PATH=/c/R/gcc-4.9.2/opt/bin:$PATH

# Extract sources
rm -Rf v8-git-mirror-3.14.5.10
tar xzvf v8-git-mirror-3.14.5.10.tar.gz
cd v8-git-mirror-3.14.5.10

# Small patch
C:/msys2-i686/usr/bin/sed -i s/winmm.lib/winmm/g tools/gyp/v8.gyp
C:/msys2-i686/usr/bin/sed -i s/ws2_32.lib/ws2_32/g tools/gyp/v8.gyp

# Note that target must be 'ia32' or 'x64' (case sensitive!)
#TARGET="ia32"
TARGET="x64"

make dependencies
GYP_GENERATORS=make \
python build/gyp_v8 \
  -Dv8_enable_i18n_support=true \
  -Duse_system_icu=1 \
  -Dconsole=readline \
  -Dcomponent=static_library \
  -Dv8_target_arch="$TARGET" \
  --generator-output=out \
  -f make

LINK=g++ make -C out BUILDTYPE=Release mksnapshot V=1 || true
LINK=g++ make -C out BUILDTYPE=Release V=1 || true
```
