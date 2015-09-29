#start buchla

#start vd_test

#run a python test

PYTHONPATH=`pwd`/../../target:$PYTHONPATH LD_LIBRARY_PATH=`pwd`/../../target:$LD_LIBRARY_PATH nosetests test.py


