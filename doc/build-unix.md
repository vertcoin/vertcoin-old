UNIX BUILD NOTES
====================

To Build
---------------------

    cd src/
    make -f makefile.unix		# Headless vertcoin

See readme-qt.rst for instructions on building vertcoin-qt, the graphical user interface.

Dependencies
---------------------

 Library       |Purpose           |Description						    				
 --------------|------------------|---------------------------------------------------------
 libssl        |SSL Support       |Secure communications
 libdb         |Berkeley DB       |Blockchain & wallet storage	
 libboost      |Boost             |C++ Library
 miniupnpc     |UPnP Support      |Optional firewall-jumping support
 libsecp256k1EC|EC Operations	  |Optimized C library for EC operations on curve secp256k1.

[miniupnpc](http://miniupnp.free.fr/) may be used for UPnP port mapping.  It can be downloaded from [here](
http://miniupnp.tuxfamily.org/files/).  UPnP support is compiled in and
turned off by default.  Set USE_UPNP to a different value to control this:

    USE_UPNP=     No UPnP support miniupnp not required
    USE_UPNP=0    (the default) UPnP support turned off by default at runtime
    USE_UPNP=1    UPnP support turned on by default at runtime

IPv6 support may be disabled by setting:

    USE_IPV6=0    Disable IPv6 support

Licenses of statically linked libraries:

 Library       |License
 --------------|---------------------------------------------------------
 Berkeley DB   |New BSD license with additional requirement that linked software must be free open source
 Boost         |MIT-like license
 miniupnpc     |New (3-clause) BSD license
 libsecp256k1  |MIT License

Dependency Build Instructions: Ubuntu & Debian
----------------------------------------------
Build requirements:

    sudo apt-get install build-essential git autoconf libtool libssl-dev libboost-all-dev libdb++-dev libgmp-dev

libsecp256k1:

    cd src/secp256k1
    ./autogen.sh
    ./configure
    make
    sudo make install

Optional:

    sudo apt-get install libminiupnpc-dev (see USE_UPNP compile flag)

Notes
-----
The release is built with GCC and then "strip vertcoind" to strip the debug
symbols, which reduces the executable size by about 90%.

Security
--------
To help make your vertcoin installation more secure by making certain attacks impossible to
exploit even if a vulnerability is found, you can take the following measures:

* Position Independent Executable
    Build position independent code to take advantage of Address Space Layout Randomization
    offered by some kernels. An attacker who is able to cause execution of code at an arbitrary
    memory location is thwarted if he doesn't know where anything useful is located.
    The stack and heap are randomly located by default but this allows the code section to be
    randomly located as well.

    On an Amd64 processor where a library was not compiled with -fPIC, this will cause an error
    such as: "relocation R_X86_64_32 against `......' can not be used when making a shared object;"

    To build with PIE, use:
    make -f makefile.unix ... -e PIE=1

    To test that you have built PIE executable, install scanelf, part of paxutils, and use:

        scanelf -e ./vertcoin

    The output should contain:
     TYPE
    ET_DYN

* Non-executable Stack
    If the stack is executable then trivial stack based buffer overflow exploits are possible if
    vulnerable buffers are found. By default, vertcoin should be built with a non-executable stack
    but if one of the libraries it uses asks for an executable stack or someone makes a mistake
    and uses a compiler extension which requires an executable stack, it will silently build an
    executable without the non-executable stack protection.

    To verify that the stack is non-executable after compiling use:
    `scanelf -e ./vertcoin`

    the output should contain:
    STK/REL/PTL
    RW- R-- RW-

    The STK RW- means that the stack is readable and writeable but not executable.
