rm -rf /tmp/failover
mkdir /tmp/failover
xterm -e "../../target/bin/failovercache_server --path=/tmp/failover | tee failovercacheserver_out 2>&1" &
sleep 1
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient1_out 2>&1" &
xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient2_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient3_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient4_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient1_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient1_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient1_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient1_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient1_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient1_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient1_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient2_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient3_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient4_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient1_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient1_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient1_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient1_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient1_out 2>&1" &
# xterm -e " ../../target/bin/distributed_transaction_log_tester --gtest_filter=*Stress* | tee failovercacheclient1_out 2>&1" &
