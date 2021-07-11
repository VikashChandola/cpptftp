#!/bin/bash
server_dir="$1/server_dir"
client_dir="$1/client_dir"
mkdir -p $server_dir
mkdir -p $client_dir
client_tests="$1/simple_client_test.sh"
echo "#!/bin/bash" > $client_tests
echo "echo \"Downloading...\"" >> $client_tests
echo "Creating sample files for test in $server_dir ..."
all_samples=""
for blocks in $(seq 0 21845 65535)
  do
    sample=sample_$((blocks * 512))
    all_samples="$all_samples $sample"
    dd status=none bs=512 count=$blocks if=/dev/urandom of=$server_dir/$sample;
    echo "$1/simple_client -H \$1 -P \$2 -W $client_dir -D $sample" >> $client_tests
    echo "mv $client_dir/$sample $client_dir/client_$sample" >> $client_tests
  done

for size in 100 513 512001
  do
    sample=sample_$size
    all_samples="$all_samples $sample"
    dd status=none bs=$size count=1 if=/dev/urandom of=$server_dir/$sample;
    echo "$1/simple_client -H \$1 -P \$2 -W $client_dir/ -D $sample" >> $client_tests
    echo "mv $client_dir/$sample $client_dir/client_$sample" >> $client_tests
  done

echo "echo \"Uploading...\"" >> $client_tests
for file in $all_samples
  do
    echo "$1/simple_client -H \$1 -P \$2 -W $client_dir/ -U client_$file" >> $client_tests
  done
echo "echo Done" >> $client_tests

chmod +x $client_tests
echo "All sample files are created"

