#!/bin/bash
server_dir="$1/server_dir/"
client_dir="$1/client_dir/"
mkdir -p $server_dir
mkdir -p $client_dir
client_tests="$1/simple_client_test.sh"
echo "#!/bin/bash" > $client_tests
echo "Creating sample files for test in $server_dir ..."
for blocks in $(seq 0 1285 65535)
  do
    sample=sample_$((blocks * 512))
    dd status=none bs=512 count=$blocks if=/dev/urandom of=$server_dir/$sample;
    echo "$1/simple_client -H 127.0.0.1 -P 12345 -W $client_dir/ -D $sample" >> $client_tests
  done
dd status=none bs=513 count=1 if=/dev/urandom of=$server_dir/sample_513;
echo "$1/simple_client -H 127.0.0.1 -P 12345 -W $client_dir/ -D sample_513" >> $client_tests
chmod +x $client_tests
echo "All sample files are created"

