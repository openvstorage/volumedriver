## Preface

The **volumedriver** sources include a testsuite to make sure the code works as expected. When building the code, you can also use this suite to check the resulting binaries are ok. Note that this does take quite some time.

## Prerequisites

If you're building using our dockerised environment (cfr. [Building with docker](volumedriver/doc/build_with_docker.md)) all prerequisites are installed and taken care off for you. 

If you are doing a [manual](volumedriver/doc/manual_building.md) build, make sure:

- **arakoon** & **alba packages** are installed (cfr ...)

        echo "deb http://packages.cloudfounders.com/apt/ unstable/" | sudo tee /etc/apt/sources.list.d/ovsaptrepo.list
        sudo apt-get update -qq
        sudo apt-get install -y --allow-unauthenticated arakoon alba
      
- **rpcbind** is running

        sudo /sbin/rpcbind
        
- **redis** is running

        sudo /usr/bin/redis-server /etc/redis/redis.conf
        
- **omniNames** is running

        sudo /usr/bin/omniNames -start -errlog /var/log/omniorb-nameserver.log || sudo /usr/bin/omniNames -errlog /var/log/omniorb-nameserver.log &
        
- you have access to **/dev/fuse**

