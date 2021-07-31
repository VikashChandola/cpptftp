if [[ $# -ne 2 ]]
then
  echo "Usage $0 <server ip> <server port>"
  exit 1
fi

for i in simple_client simple_server
do
if [[ ! -f $i ]]
then
  echo "$i file not found"
  echo "Build $i, (meson compile)"
  exit 1
fi
done

pidof simple_server > /dev/null
if [[ $? -gt 0 ]]
then
  echo "simple_server is not running"
  exit 1
fi

