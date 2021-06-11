# Unit Test Run Guide
This document explains how to run unit tests. Some tests can be run as standalone binary whereas others require
intervention from user. All tests below are meant to be executed from meson build directory.

## tftp framing library
* Run `tftp_frame_test` binary. Tests run should pass with no errors.

## tftp client library
  1. Run tftp server
      ```
      $ ./bin/pyTFTP/server.py ./work_dir/ -H 127.0.0.1 -p 12345
      ```
  2. run test binary to validate download. `tftp_client_test` takes two argument ip and port number.
      ```
      $ ./tftp_client_test -- 127.0.0.1 12345
      ```
  3. Testing network interruption. Run same as previous test but kill the server while download is still going on.
  This should fail the test with error_code 2. Below is sample output.
      ```
      $ ./tftp_client_test -- 127.0.0.1 12345
      Running 7 test cases...../test/tftp_client.cpp(71): fatal error: in "download_existing_file_of_known_size/_6": Download failed with error code :2
      Failure occurred in a following context:
        block_size = 8192; count = 4095;
      *** 1 failure is detected in the test module "tftp client download only"
      ``
