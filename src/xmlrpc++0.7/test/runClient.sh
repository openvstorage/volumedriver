PORT=9090
REPEAT=40
valgrind ./testclient --gtest_filter=*TortureTest* --torture=$REPEAT --stop=true --port=$PORT
