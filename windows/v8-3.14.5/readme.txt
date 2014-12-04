Notes from Jeroen (Dec 2014)
----------------------------

- We need libv8 around version 3.14. This is the version node
  currently uses. More recent versions of v8 probably won't work.
- The libv8 build does not play nice with Rtools multilib (-m64).
- MSYS2 tool chain did not work either because it uses seh0/dwarf
  exceptions but Rtools (and hence R) currently uses sj0 only.
- GYP needs a mingw build of python to work. Python for windows
  generates paths with backslashes which messes up everything.
- SVN is required for GYP. The windows version (Turtoise) is fine.

Requied Tools
-------------

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
 - Download https://github.com/v8/v8/archive/3.14.5.tar.gz
 - Run the build script.
