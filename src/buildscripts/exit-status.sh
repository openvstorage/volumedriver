#! /bin/bash
set -ux

function catch_errors_maybe_generate_xml {
    EXIT_STATUS=$1
    TEST_BINARY_NAME=$2
    TEST_XML_FILE=$3
    if [ "$EXIT_STATUS" -eq "0" ]
    then
	echo "Normal exit of test, should have succeeded"
	true
    elif [ "$EXIT_STATUS" -eq "1" ]
    then
	echo "Abnormal exit, some tests have failed"
	true
    else
	echo "Abnormal exit status: $EXIT_STATUS"
	if [ -f $TEST_XML_FILE ]
	then
	    echo "test output file was generated, not regenerating it"
	    false
	else
	    echo "Will generate the test report for jenkins"

	    cat > $TEST_XML_FILE <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<testsuites tests="1" failures="1" disabled="0" errors="0" time="0.001" name="$TEST_BINARY_NAME">
<testsuite name="$TEST_BINARY_NAME" tests="1" failures="1" disabled="0" errors="0" time="0.001">
<testcase name="testerCrashedAbnormally" status="run" time="0.0.1" classname="$TEST_BINARY_NAME">
<failure type="Abnormal exit $EXIT_STATUS" message="Tester exited with status $EXIT_STATUS"/>
</testcase>
</testsuite>
</testsuites>
EOF
	    false
	fi
    fi
}
