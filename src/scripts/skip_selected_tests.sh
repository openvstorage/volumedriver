function skip_selected_tests {
    for test_ in ${SKIP_TESTS:-""}
    do
	if [ "x${test_}" == "x$1" ]
	then
	# this doesn't work -> you need to generate the tester result file explicitly
	    echo "not running test as $1 tests are supposed to be skipped"
	    touch ${TEST_OUTPUT_FILE}
	    exit 0
	fi
    done
}
