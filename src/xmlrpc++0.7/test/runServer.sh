PORT=9090
REPEAT=5
valgrind --leak-check=full --track-fds=yes ./testserver --port=$PORT --mt=true