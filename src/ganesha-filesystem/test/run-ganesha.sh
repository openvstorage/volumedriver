#! /bin/sh

@buildtoolsdir@/bin/ganesha.nfsd args -p @prefix@/ganesha.pid -f @prefix@/share/ganesha-filesystem/test/ganesha.conf -L @prefix@/ganesha.log
