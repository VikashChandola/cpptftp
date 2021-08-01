#!/bin/bash
if [[ $# -ne 2 ]]
then
  echo "$0 <build directory path> <this scripts directory path>"
  exit 1
fi
server_dir="$1/server_dir"
client_dir="$1/client_dir"
mkdir -p $server_dir
mkdir -p $client_dir
client_tests="$1/test_all.sh"
echo "#!/bin/bash" > $client_tests
cat $2/frag_pre_check.sh >> $client_tests
echo "Creating sample files for test in $server_dir ..."
all_samples=""
for blocks in $(seq 0 21845 65535)
  do
    sample=sample_$((blocks * 512))
    all_samples="$all_samples $sample"
    dd status=none bs=512 count=$blocks if=/dev/urandom of=$server_dir/$sample;
  done

for size in 100 513 512001
  do
    sample=sample_$size
    all_samples="$all_samples $sample"
    dd status=none bs=$size count=1 if=/dev/urandom of=$server_dir/$sample;
  done
echo "All samples created"

echo "echo \"Downloading...\"" >> $client_tests
for file in $all_samples
  do
    echo "$1/simple_client -H \$1 -P \$2 -W $client_dir/ -D $file > /dev/null 2>&1 &" >> $client_tests
  done

echo "echo Waiting for downloads to complete..." >> $client_tests
echo "wait" >> $client_tests
for file in $all_samples
  do
    echo "mv $client_dir/$file $client_dir/client_$file" >> $client_tests
  done
echo "echo Downloads completed..." >> $client_tests

echo "echo \"Uploading...\"" >> $client_tests
for file in $all_samples
  do
    echo "$1/simple_client -H \$1 -P \$2 -W $client_dir/ -U client_$file > /dev/null 2>&1 &" >> $client_tests
  done
echo "echo Waiting for upload to complete..." >> $client_tests
echo "wait" >> $client_tests
echo "echo Uploads completed..." >> $client_tests

cat $2/frag_checker.sh >> $client_tests
echo "echo Download upload test passed" >> $client_tests

chmod +x $client_tests
echo "All sample files are created"

