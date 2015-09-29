#! /bin/sh

# to run this test without having to enter the password each time
# add your id_rsa.pub public key (from ~/.ssh to the authorized_keys
# on the server machine

REST_TEST_SERVER=172.19.44.242

cat > rest-qshell-test-setup <<EOF
#!/opt/qbase3/qshell -f
from pymonkey.InitBase import q
q.dsstestframework.tearDown()
q.dsstestframework.setUp(enableREST=True, logLevel="INFO")
q.dss.clientdaemons.stop()
#temporary fix for issue Arne encountered: DSS-599
with open("/opt/qbase3/cfg/dss/clientdaemons/0.cfg", "a") as f:
    f.write("max_object_syncstore_connections_per_object_syncstore = 100\n")
q.dss.clientdaemons.start()
q.dss.manage.setPermissions("/manage", "everyone", ["Read", "Create", "Delete", "List", "Update"])
import time
#to get permissions right
time.sleep(30)
EOF

chmod +x rest-qshell-test-setup
scp rest-qshell-test-setup root@$REST_TEST_SERVER:/tmp
rm rest-qshell-test-setup
ssh root@$REST_TEST_SERVER /tmp/rest-qshell-test-setup

#cat > volumedriverTestLogging <<EOF
#log4j.rootLogger=FATAL, A1
#log4j.appender.A1=org.apache.log4j.ConsoleAppender
#log4j.appender.A1.layout=org.apache.log4j.PatternLayout
#log4j.appender.A1.layout.ConversionPattern=%-5p %c %x - %m [%t]%n
#EOF

cat > resttestbackend.cfg << EOF
backend=REST
host=$REST_TEST_SERVER
port=8080
EOF

set -x
PATH=../target/bin:$PATH backend_test --backend-config-file resttestbackend.cfg "$@"

cat > rest-qshell-test-teardown <<EOF
#!/opt/qbase3/qshell -f
from pymonkey.InitBase import q
q.dsstestframework.tearDown()
EOF

chmod +x rest-qshell-test-teardown
scp rest-qshell-test-teardown root@172.19.44.242:/tmp
rm rest-qshell-test-teardown
ssh root@172.19.44.242 /tmp/rest-qshell-test-teardown


# Local Variables: **
# mode: sh **
# compile-command: "scons -D --kernel_version=system --ignore-buildinfo" **
# End: **
