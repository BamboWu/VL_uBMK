#!/bin/bash
#SBATCH -J perf_shopping                 # job name
#SBATCH -o perf_shopping.o.%j            # file to dump stdout and stderr
#SBATCH -n 1                             # number of MPI jobs per node
#SBATCH -N 1                             # number of nodes
#SBATCH -p normal                        # partition/queue
#SBATCH -t 00:30:00                      # time limit HH:MM:SS
#SBATCH -A Frontera-Training             # Allocation (project)
#SBATCH --mail-user=qw2699@utexas.edu    # email address to send notification
#SBATCH --mail-type=end                  # trigger emailing at start/end/all

#module load boost/1.69

ROOT_DIR=/export/bambo/bmkvl/VL_uBMK
PLATFORM=jupiter1
HW_INFO=$ROOT_DIR/scripts/${PLATFORM}.cfg

# sanity check
if [ ! -e $HW_INFO ];
then
    echo -e "\033[91mCannot find hardware information: $HW_INFO!\033[0m"
    exit -1
elif [ ! -z $1 ]; # with any argument, sanity check only
then
    echo -e "\033[92mCommands to execute:\033[0m"
    ECHO=true
    EVAL=echo
else
    ECHO=echo
    EVAL=eval
fi

source $HW_INFO

TIMESTAMP=`date +"%d%b%H%M"`
BUILD_DIR=$ROOT_DIR/build
DATA_DIR=$ROOT_DIR/data/perf_shopping_${PLATFORM}_${TIMESTAMP}

# prepare directories
$EVAL "mkdir -p $BUILD_DIR $DATA_DIR"

# make the executables
$EVAL "cd $BUILD_DIR"
$EVAL "cmake ../"
$EVAL "make atomicFSh atomicPad directFSh shuffler"

# processing hardware information
CPU_LIST=`echo $CPU_PREFERENCE | tr ',' ' '`
CPU_LIST=($CPU_LIST)
MAX_THREADS=$(( $NUM_SOCKETS * $CORES_PER_SOCKETS * $THREADS_PER_CORES ))
MAX_CORES=$(( $NUM_SOCKETS * $CORES_PER_SOCKETS ))
$EVAL "lscpu"

# compose test cases (1 writer 2^N readers)
WRITER_BALANCE=1024000
READER_BALANCE=6553600000
ntests=0
nreaders=0
while [[ $((1 + $nreaders)) -lt $MAX_THREADS ]];
do
    tests[$ntests]=""
    for i in `seq 0 $nreaders`;
    do
        # append CPU id
        tests[$ntests]="${tests[$ntests]} ${CPU_LIST[$i]}"
        if [[ $i -eq $ntests ]];
        then
            # append thread name
            tests[$ntests]="${tests[$ntests]} MMFan${CPU_LIST[$i]}"
            # append thread type ('M' is MMFan) and balance
            tests[$ntests]="${tests[$ntests]} M $WRITER_BALANCE"
        else
            # append thread name
            tests[$ntests]="${tests[$ntests]} Watcher${CPU_LIST[$i]}"
            # append thread type ('W' is Watcher) and balance
            tests[$ntests]="${tests[$ntests]} W $READER_BALANCE"
        fi
    done
    nreaders=$((2**$ntests))
    ntests=$(($ntests + 1))
    # shift CPU_LIST
    #CPU_LIST[$MAX_THREADS]=${CPU_LIST[0]}
    #for i in `seq 0 $(($MAX_THREADS - 1))`;
    #do
    #    CPU_LIST[$i]=${CPU_LIST[$(($i + 1))]}
    #done
done

# profile
PERF_CMD="perf stat -e $PERF_COUNTERS"
#PERF_EVT=`awk '/PERF_COUNTERS=/ {print $2}' $HW_INFO`
ATOMICFSH=$BUILD_DIR/apps/shopping/atomicFSh
ATOMICPAD=$BUILD_DIR/apps/shopping/atomicPad
DIRECTFSH=$BUILD_DIR/apps/shopping/directFSh
SHUFFLER=$BUILD_DIR/apps/shuffler/shuffler
for i in `seq 0 $(($ntests - 1))`;
do
    $ECHO "$SHUFFLER SpD $MAX_CORES $LLC_CAPACITY"
    $EVAL "$SHUFFLER SpD $MAX_CORES $LLC_CAPACITY"
    $ECHO "$SHUFFLER FSh $MAX_CORES $LLC_CAPACITY"
    $EVAL "$SHUFFLER FSh $MAX_CORES $LLC_CAPACITY"
    $ECHO "$PERF_CMD -o $DATA_DIR/perf$i.atomicFSh $ATOMICFSH ${tests[$i]}"
    $EVAL "$PERF_CMD -o $DATA_DIR/perf$i.atomicFSh $ATOMICFSH ${tests[$i]}"
    $ECHO "$SHUFFLER SpD $MAX_CORES $LLC_CAPACITY"
    $EVAL "$SHUFFLER SpD $MAX_CORES $LLC_CAPACITY"
    $ECHO "$SHUFFLER FSh $MAX_CORES $LLC_CAPACITY"
    $EVAL "$SHUFFLER FSh $MAX_CORES $LLC_CAPACITY"
    $ECHO "$PERF_CMD -o $DATA_DIR/perf$i.atomicPad $ATOMICPAD ${tests[$i]}"
    $EVAL "$PERF_CMD -o $DATA_DIR/perf$i.atomicPad $ATOMICPAD ${tests[$i]}"
    $ECHO "$SHUFFLER SpD $MAX_CORES $LLC_CAPACITY"
    $EVAL "$SHUFFLER SpD $MAX_CORES $LLC_CAPACITY"
    $ECHO "$SHUFFLER FSh $MAX_CORES $LLC_CAPACITY"
    $EVAL "$SHUFFLER FSh $MAX_CORES $LLC_CAPACITY"
    $ECHO "$PERF_CMD -o $DATA_DIR/perf$i.directFSh $DIRECTFSH ${tests[$i]}"
    $EVAL "$PERF_CMD -o $DATA_DIR/perf$i.directFSh $DIRECTFSH ${tests[$i]}"
done
$EVAL "lscpu"
