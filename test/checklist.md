Following are the list of test cases.

### libtftpframe.so
- [X] tftp read request frame creation
- [X] tftp write request frame creation
- [X] tftp data frame creation
- [X] tftp ack frame creation
- [X] tftp error frame creation

- [ ] parse read request tftp frame
- [ ] parse write request tftp frame
- [ ] parse data tftp frame
- [ ] parse ack tftp frame
- [ ] parse error tftp frame

- [ ] parse invalid tftp packet
- [ ] parse partial read request tftp frame
- [ ] parse partial write request tftp frame
- [ ] parse partial data tftp frame
- [ ] parse partial ack tftp frame
- [ ] parse partial error tftp frame

### libtftpclient.so
- [X] Download file of size 0 bytes
- [X] Download file of size 100 bytes
- [X] Download file of size 512 bytes
- [X] Download file of size 513 bytes
- [X] Download file of size 33553920 bytes
- [X] Download file of size 33553921 bytes
- [ ] Download file with slow tftp server ie server responds with some delay
- [ ] Download file where tftp packets are not received in single udp packet
- [ ] Download file that is not available in server
- [X] Network interruption while downloading(handelling retransmission)

- [ ] Upload file of size 0 bytes
- [ ] Upload file of size 100 bytes
- [ ] Upload file of size 512 bytes
- [ ] Upload file of size 513 bytes
- [ ] Upload file of size 33553920 bytes
- [ ] Upload file of size 33553921 bytes
- [ ] Upload file with slow tftp server ie server responds with some delay
- [ ] Upload file where tftp packets are not received in single udp packet
- [ ] Upload file that is not available in server
- [ ] Upload interruption while downloading(handelling retransmission)
- [ ] Network interruption while uploading(handelling retransmission)

### libtftpserver.so
- [ ] Serve file of size 0 bytes
- [ ] Serve file of size 100 bytes
- [ ] Serve file of size 512 bytes
- [ ] Serve file of size 513 bytes
- [ ] Serve file of size 33553920 bytes
- [ ] Serve file of size 33553921 bytes
- [ ] Serve a file that doesn't exit
- [ ] Serve many files concurrently to many clients
- [ ] Serve a single file concurrently to multiple clients
- [ ] Serving client who doesn't send ack all the times(ie retransmission scenario)
