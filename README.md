cpptftp is a tftp client and server implementation.

## Compilation
Compilation needs at least 1vCPU and 2GB of RAM.
### Dependencies
Following libraries are required for compilation
1. boost::asio    : For asynchronous IO
2. boost::test    : For unit tests

These are header only library and doesn't bring any runitme dependency.

#### Arch Linux
```
```

#### Ubuntu
```
$ sudo apt-get update
$ sudo apt-get -y upgrade
$ sudo apt-get -y install build-essential meson libboost-all-dev
```

### compilation
```
$ git clone https://github.com/VikashChandola/cpptftp.git
$ cd cpptftp
$ meson build
$ cd build
$ meson compile
```
If meson compile fails then use ninja directly for compilation example `ninja install`.

## Usage
`simple_client` and `simple_server` gets generated during compilation.
### Server
```
$ ./simple_server -h
Usage :./simple_server -H <host address> -P <port number> -W <working directory>
```
simple_server takes Host address on which tftp server should be set up, udp port number and working directory as
command line arguments. In following exmaple server will listen from all the clients on UDP port 12345 with working
directory and server_dir. All the file paths are taken relative to working directory. If tftp client requests for xyz
file then ./server_dir/xyz will be served.

```
$ ./simple_server -H 0.0.0.0 -P 12345 -W ./server_dir
Host address            :0.0.0.0
Host Port               :12345
Working directory       :./server_dir
Starting distribution on 0.0.0.0:12345
```

### Client
`simple_client` can download and upload from remote tftp server
```
$ ./simple_client -h
Usage :./simple_client -H <server address> -P <port number> -W <working directory>
```
Upload and download operations are done relative to working directory.
```
$ ./simple_client -H 127.0.0.1 -P 12345 -W ./client_dir -D sample_33553920
TFTP Server address     :127.0.0.1
TFTP Server Port        :12345
Working directory       :./client_dir
Operation               :Download
File                    :sample_33553920
Download status         : 0
```
`simple_client` does one transaction(either download or upload) per invocation. client and server can also be used as
shared libraries. Refer implementation of `simple_client` and `simple_server` implementation to know more.

