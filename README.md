Unique Race
===========

The Unique Race modification of DDNet

Building
--------

To compile Unique Race, execute the following commands in the source root:

    mkdir build
    cd build
    cmake -DCLIENT=OFF -DMYSQL=ON ..
    make -j$(nproc) DDNet-Server

On Arch Linux the MariaDB C++ connector is available as the AUR package `mysql-connector-c++`.

Cross-compiling for Windows
---------------------------

On Arch Linux install `mingw-w64-gcc`.

    mkdir buildwin
    cd buildwin
    cmake -DCLIENT=OFF -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/mingw64.toolchain ..
    make -j$(nproc) DDNet-Server

Copy to server for download.

    scp DDNet-Server.exe uniqueweb@uniqueclan.net:unique-web/public/UniqueRaceServer.exe
