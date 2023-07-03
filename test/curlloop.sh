# loop forever for an http code
# ALLOWTIMEOUT allows to ignore timeouts that might occur when the JVM is hanging.
# parameters list of http_code 000 = timeout.
#
codes=""
for var in "$@"
do
    codes="$codes $var"
done

while true
do
  found=0
  http_code=`curl -s -m10 -o /dev/null -w "%{http_code}" http://localhost:8000/testapp/test.jsp`
  for var in "$@"
  do 
    if [ "${http_code}" == "${var}" ]; then
      found=1
      break
    fi
  done
  if [ ${found} -eq 0 ]; then
    echo "ERROR got: ${http_code} expects one of ${codes} `date`"
    exit 1
  fi

  sleep 5
done
