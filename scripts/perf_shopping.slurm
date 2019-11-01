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
HW_INFO=$ROOT_DIR/scripts/jupiter.cfg

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
DATA_DIR=$ROOT_DIR/data/${TIMESTAMP}_perf_shopping

# prepare directories
$EVAL "mkdir -p $BUILD_DIR $DATA_DIR"

# make the executables
$EVAL "cd $BUILD_DIR"
$EVAL "cmake ../"
$EVAL "make atomicFSh atomicPad directFSh"

# processing hardware information
CPU_LIST=`echo $CPU_PREFERENCE | tr ',' ' '`
CPU_LIST=($CPU_LIST)
MAX_THREADS=$(( $NUM_SOCKETS * $CORES_PER_SOCKETS * $THREADS_PER_CORES ))

# compose test cases (1 writer 2^N readers)
ntests=0
nthreads=1
tests[$ntests]="${CPU_LIST[0]} MMFan M 1024"
while [[ $(($nthreads * 2)) -lt $MAX_THREADS ]];
do
    ntests=$(($ntests + 1))
    tests[$ntests]=${tests[0]}
    nthreads=1
    for i in `seq 1 $((2**($ntests - 1)))`;
    do
        # append CPU id
        tests[$ntests]="${tests[$ntests]} ${CPU_LIST[$nthreads]}"
        # append thread name
        tests[$ntests]="${tests[$ntests]} Watcher${CPU_LIST[$nthreads]}"
        # append thread type ('W' is Watcher) and balance
        tests[$ntests]="${tests[$ntests]} W 8192"
        nthreads=$(($nthreads + 1))
    done
done

#for i in `seq 0 $ntests`;
#do
#    echo ${tests[$i]}
#done

# profile
PERF_CMD="perf stat -e $PERF_COUNTERS"
#PERF_EVT=`awk '/PERF_COUNTERS=/ {print $2}' $HW_INFO`
ATOMICFSH=$BUILD_DIR/apps/shopping/atomicFSh
ATOMICPAD=$BUILD_DIR/apps/shopping/atomicPad
DIRECTFSH=$BUILD_DIR/apps/shopping/directFSh
for i in `seq 0 $ntests`;
do
    $ECHO "$PERF_CMD -o $DATA_DIR/perf$i.atomicFSh $ATOMICFSH ${tests[$i]}"
    $EVAL "$PERF_CMD -o $DATA_DIR/perf$i.atomicFSh $ATOMICFSH ${tests[$i]}"
    $ECHO "$PERF_CMD -o $DATA_DIR/perf$i.atomicPad $ATOMICPAD ${tests[$i]}"
    $EVAL "$PERF_CMD -o $DATA_DIR/perf$i.atomicPad $ATOMICPAD ${tests[$i]}"
    $ECHO "$PERF_CMD -o $DATA_DIR/perf$i.directFSh $DIRECTFSH ${tests[$i]}"
    $EVAL "$PERF_CMD -o $DATA_DIR/perf$i.directFSh $DIRECTFSH ${tests[$i]}"
done
