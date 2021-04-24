Usage API for client
* create client with 
  tftp_client_h client = tftp_client::create([asio::io_context], [udp::endpoint])
* Following opeartion can be performed on tftp_client object
  * Get file from remote end and write it to output stream
    client->download_file([remote file],[std::ostream], [completion callback])
    Download [remote file] and writes it to ostream. Once download is finished
    [completion callback] is invoked.
  * Get file from remote end and write it to local file
    client->download_file([remote file], [local file], [completion callback])
    Download [remote file] and writes it [local file]. Once download is finished
    [completion callback] is invoked.
  * Upload file to remote server using
    client->upload_file([std::istream], [completion callback])
    Data gets read from [std::istream] and written to remote location.
    [completion callback] will be called on completion of operation or error.
  * signature of [completion callback] is as follow
    void callback_function(cpptftp_error_code e);
  * Refer tftp_client.hpp for all erorr codes.

