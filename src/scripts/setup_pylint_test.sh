

function pylint_test {

TEST_OUTPUT_DIR=${prefix?}/tests
TEST_OUTPUT_FILE=${TEST_OUTPUT_DIR}/$1_pylint.out

mkdir -p ${TEST_OUTPUT_DIR}
rm -f ${TEST_OUTPUT_FILE}


export PYTHONPATH=${prefix}/lib/python@PYTHON_VERSION@/dist-packages
export LOG_DIR=/tmp


skip_selected_tests "pylint"


pylint --disable=$DISABLED_INFO --disable=$DISABLED_CONVENTIONS --disable=$DISABLED_REFACTORS --include-ids=yes -f parseable $1 | tee ${TEST_OUTPUT_FILE}
}