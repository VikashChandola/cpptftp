echo "echo Comparing files..."
cd server_dir
for i in sample_*
do
  cmp $i client_$i
  if [[ $? -gt 0 ]]
  then
    echo "Mismatch in file $i"
    exit 1
  fi
done
for i in sample_*
do
  rm client_$i
done
cd -
rm -rf client_dir/*
