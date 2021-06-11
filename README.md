cpptftp intends to create a tftp client and server application and libraries.

Project is dependent on following libraries. This excludes libc, pthread
ecosystem.
1. boost::asio    : For asyncronous IO
2. nlohmann-json  : For client application configuration parsing
3. m4tx/pyTFTP    : This tftp is used as remote server for Unit tests

## Compilation from source
Compilation needs at least 1vCPU and 2GB of RAM. That is t2.small or higher if you speak in aws.
### Dependencies
Following libraries are required for compilation
1. boost::asio    : For asyncronous IO
2. nlohmann-json  : For client application configuration parsing
Both of these libraries are needed only for compilation.

#### Arch Linux
```
```

#### Ubuntu
```
$ sudo apt-get update
$ sudo apt-get -y upgrade
$ sudo apt-get -y install build-essential meson libboost-all-dev nlohmann-json3-dev
```

### compilation
```
$ git clone https://github.com/VikashChandola/cpptftp.git
$ cd cpptftp
$ meson builddir
$ cd builddir
$ meson compile
```
If meson compile fails then use ninja directly for compilation example `ninja install`.

