#!/bin/bash
set -eux

SCRIPTDIR=$(dirname $(readlink -f $0))

export TEMP=${TEMP:-/tmp}

export ARAKOON_BASE_DIR=${ARAKOON_BASE_DIR:-${TEMP}/arakoon}
ARAKOON_BIN=${ARAKOON_BIN:-/usr/bin/arakoon}

ARAKOON_CONFIG_TEMPLATE=${ARAKOON_CONFIG_TEMPLATE:-${SCRIPTDIR}/arakoon_cfg_template.ini}

ALBA_BASE_DIR=${ALBA_BASE_DIR:-${TEMP}/alba}
ALBA_PLUGIN_HOME=${ALBA_PLUGIN_HOME:-/usr/lib/alba}
ALBA_BIN=${ALBA_BIN:-/usr/bin/alba}

[ -d ${TEMP} ] || mkdir -p ${TEMP}
[ -d ${ARAKOON_BASE_DIR} ] || mkdir -p ${ARAKOON_BASE_DIR}

ARAKOON_CONFIG_FILE=${ARAKOON_CONFIG_FILE:-${ARAKOON_BASE_DIR}/test.ini}

# create the arakoon config file from the template, using the settings just set
envsubst <${ARAKOON_CONFIG_TEMPLATE} >${ARAKOON_CONFIG_FILE}

ARAKOON_NODES=$(awk -F '=' '/^cluster[ ]*=/ { gsub(",", " "); print $2 }' ${ARAKOON_CONFIG_FILE})

for node in ${ARAKOON_NODES}
do
    # FIXME Trim node whitespace
    [ -d "${ARAKOON_BASE_DIR}/$node" ] && rm -rf "${ARAKOON_BASE_DIR}/$node"
    mkdir -p ${ARAKOON_BASE_DIR}/$node
    ln -s ${ALBA_PLUGIN_HOME}/nsm_host_plugin.cmxs ${ARAKOON_BASE_DIR}/$node/nsm_host_plugin.cmxs
    ln -s ${ALBA_PLUGIN_HOME}/albamgr_plugin.cmxs ${ARAKOON_BASE_DIR}/$node/albamgr_plugin.cmxs
    nohup ${ARAKOON_BIN} --node $node -config ${ARAKOON_CONFIG_FILE} >> \
        ${ARAKOON_BASE_DIR}/$node/nohup.output 2>&1 &
    echo $! > ${ARAKOON_BASE_DIR}/$node/nohup.pid
done

sleep 5 ## give arakoon some time to select master node

who_master_retries=10
running=0
while [ $who_master_retries -gt 0 ]; do
    ${ARAKOON_BIN} -config ${ARAKOON_CONFIG_FILE} --who-master && running=1 || true
    if [ "$running" -ne 0 ]; then
        break
    fi
    who_master_retries=$((who_master_retries-1))
    sleep 5
    ## in case arakoon had anything to say; show it now...
    for f in ${ARAKOON_BASE_DIR}/*/nohup.output
    do
      echo "=== $f ==="
      cat $f
    done
done

if [ "$running" -eq 0 ]; then
    echo "arakoon cluster setup should not take so long."
    exit 2
fi

# Start ALBA proxy
ALBA_PROXY_HOME=${ALBA_BASE_DIR}/proxy
ALBA_PROXY_CACHE=${ALBA_PROXY_HOME}/fragment_cache
ALBA_PROXY_CFG_FILE=${ALBA_PROXY_HOME}/proxy.cfg
ALBA_PROXY_MGR_CFG_FILE=${ALBA_PROXY_HOME}/albamgr.cfg

mkdir -p ${ALBA_PROXY_CACHE}

cp ${ARAKOON_CONFIG_FILE} ${ALBA_PROXY_MGR_CFG_FILE}

cat <<EOF > ${ALBA_PROXY_CFG_FILE}
{
     "port" : 10000,
     "albamgr_cfg_file" : "${ALBA_PROXY_MGR_CFG_FILE}",
     "log_level" : "debug",
     "fragment_cache_dir" : "${ALBA_PROXY_CACHE}",
     "manifest_cache_size" : 100000,
     "fragment_cache_size" : 100000000
}

EOF

nohup ${ALBA_BIN} proxy-start --config=${ALBA_PROXY_CFG_FILE} >> ${ALBA_PROXY_HOME}/proxy.out 2>&1 &
echo $! > ${ALBA_PROXY_HOME}/nohup.pid

# Start maintenance agents
ALBA_MAINTENANCE_HOME=${ALBA_BASE_DIR}/maintenance
ALBA_MAINTENANCE_CFG_FILE=${ALBA_MAINTENANCE_HOME}/maintenance.cfg

mkdir -p ${ALBA_MAINTENANCE_HOME}

cat <<EOF > ${ALBA_MAINTENANCE_CFG_FILE}
{
     "albamgr_cfg_file" : "${ARAKOON_CONFIG_FILE}",
     "log_level" : "debug"
}

EOF

n_agents=1
n_counter=0
while [  $n_counter -lt $n_agents ]; do
    nohup ${ALBA_BIN} maintenance --modulo ${n_agents} --remainder $n_counter \
    --config=${ALBA_MAINTENANCE_CFG_FILE} >> \
    ${ALBA_MAINTENANCE_HOME}/maintenance_"$n_counter"_"${n_agents}".out 2>&1 &
    echo $! > ${ALBA_MAINTENANCE_HOME}/nohup.pid
    n_counter=$((n_counter+1))
done

# Register NSM host
${ALBA_BIN} add-nsm-host ${ARAKOON_CONFIG_FILE} --config ${ARAKOON_CONFIG_FILE}

# Start OSDs
KIND="ASD"
ASD_BASE_DIR=${ALBA_BASE_DIR}/asd
nodeid_prefix=$(cat /proc/sys/kernel/random/uuid)
N=12
a_counter=0
while [  $a_counter -lt $N ]; do
    ASD_PATH=$(printf "${ASD_BASE_DIR}/%02d" ${a_counter})
    CFG_PATH=${ASD_PATH}/cfg.json
    port=$((8000 + $a_counter))
    nodeid=$((port / 4))
    limit=99

    mkdir -p ${ASD_PATH}

    cat <<EOF > ${CFG_PATH}
    {
        "port": ${port},
        "node_id": "${nodeid_prefix}_${nodeid}",
        "home": "${ASD_PATH}",
        "log_level": "debug",
        "asd_id": "${port}_${nodeid}_${nodeid_prefix}",
        "limit": ${limit},
        "__sync_dont_use": false
    }

EOF

    nohup ${ALBA_BIN} asd-start --config ${CFG_PATH} >> ${ASD_PATH}/output 2>&1 &
    echo $! > ${ASD_PATH}/nohup.pid
    a_counter=$((a_counter+1))
done

# Claim OSDs
claim_osd_retries=0
claimed_osds=0
listed=0
while [ $claimed_osds -ne $N -a $claim_osd_retries -lt 60 ]; do
    json_res=$(${ALBA_BIN} list-available-osds --config ${ARAKOON_CONFIG_FILE} --to-json 2>/dev/null)
    json_len=$(echo $json_res | python -c 'import json,sys; print len(json.loads(sys.stdin.read())["result"])')
    if [ "$json_len" -ne ${N} ];then
        claim_osd_retries=$((claim_osd_retries+1))
        sleep 1
    else
        for i in `seq 1 ${json_len}`; do
            no=$((i-1))
            nodeid=$(echo $json_res | python -c "import json,sys;print json.loads(sys.stdin.read())[\"result\"][$no][\"node_id\"];")
            longid=$(echo $json_res | python -c "import json,sys;print json.loads(sys.stdin.read())[\"result\"][$no][\"long_id\"];")

            if [[ "${nodeid}" =~ "${nodeid_prefix}" ]]; then
                ${ALBA_BIN} claim-osd --config ${ARAKOON_CONFIG_FILE} --long-id ${longid} --to-json
                claimed_osds=$((claimed_osds+1))
            fi
        done
    fi
done

if [ "$claim_osd_retries" -eq 60 ]; then
    echo "cannot claim all OSDs."
    exit 2
fi

# Create 'demo' namespace
${ALBA_BIN} create-namespace "demo" --config ${ARAKOON_CONFIG_FILE}

# Wait here forever for our friend jenkins
exec sleep infinity
