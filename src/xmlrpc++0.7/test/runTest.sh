PORT=9090
REPEAT=1
xterm -e "./xmlrpcserver_mt --port=$PORT" &
sleep 1
#xterm -e "./xmlrpcclient --gtest_filter=*TortureTest* --gtest_break_on_failure --gtest_repeat=$REPEAT -torture=20 --port=$PORT && echo ok && sleep 20" &
xterm -e "./xmlrpcclient --gtest_filter=*TortureTest* --gtest_break_on_failure --gtest_repeat=$REPEAT --torture=$REPEAT --stop=true --port=$PORT && echo ok && sleep 20" &
#xterm -e "./xmlrpcclient --gtest_break_on_failure --gtest_repeat=$REPEAT --port=$PORT && echo ok && sleep 20" &