#!/bin/sh

# Threaded test config
# Nokia wants 64 streams * 3 buffers each (=192 buffers) (src + dest = 2*192)
ITER=10
#BUF_SIZES="6580 6592"
BUF_SIZES="6580"

#gpdma is 4 channels...
# option #1: 48 (sg_buffers) * 1 threads_per_channel * 4 channels = 192
# option #2: 24 (sg_buffers) * 2 threads_per_channel * 4 channels = 192
# [...]

SG_BUFFERS=2	#3  #6 #12 #24 #48
THREADS_PER_CHAN=1	#16 #8 #4  #2  #1 

#test_channel can be 'null char' - then all (4) channels in use
#or defined dmaNchanY, N,Y=0 thr 1, eg. 'dma0chan0'
TEST_CHANNEL=dma0chan1
#TEST_CHANNEL='\0' #null char

L3_LOCKING_ARRAY=(
"l3_lock_to_l3_lock"
"l3_lock_to_l3_unlock"
"l3_unlock_to_l3_unlock"
)
L3_LOCKING_ARRAY=(l3_lock_to_l3_lock)

#LM_BENCH=(no yes)
LM_BENCH=(no)

LOGFILE=$1
if [[ -f $LOGFILE ]]; then
	LOGGING=yes
else
	LOGGING=no
	echo "No logfile. Logging to stdout"
fi

source ./lmbench.sh

log() {
	if [[ $LOGGING == yes ]]; then
		echo -e "$1" | tee -a $LOGFILE
	else
		echo -e "$1"
	fi
}

dmesg -C

# 'cat on wait' below blocks for more than 120 sec - ignore.
echo 0 > /proc/sys/kernel/hung_task_timeout_secs
cd /workspace/sw/mbykowsx/lionfish/kernel/drivers/dma
if lsmod | grep -q dmatest; then
	rmmod dmatest
fi
insmod ./dmatest.ko

echo $ITER > /sys/module/dmatest/parameters/iterations
echo $THREADS_PER_CHAN > /sys/module/dmatest/parameters/threads_per_chan
echo $SG_BUFFERS > /sys/module/dmatest/parameters/sg_buffers
echo -ne $TEST_CHANNEL > /sys/module/dmatest/parameters/channel
echo Y > /sys/module/lsi_dma32/parameters/rif_mode


log "#############################################"
log "Test started `date`"
log "#############################################"

for IS_LMBENCH in ${LM_BENCH[@]}; do
	[[ $IS_LMBENCH == yes ]] && run_lmbench
	for L3_LOCKING in ${L3_LOCKING_ARRAY[@]}; do
		for BUF_SIZE in ${BUF_SIZES[@]}; do
			echo $L3_LOCKING > /sys/module/dmatest/parameters/lock
			echo $BUF_SIZE > /sys/module/dmatest/parameters/test_buf_size

			sleep 0.5

			#echo kicks the test off if idle. If running will return error
			start=`date +%s`
			echo 1 > /sys/module/dmatest/parameters/run

			#cat is blocking until all threads complete
			cat /sys/module/dmatest/parameters/wait > /dev/null 2>&1
			end=`date +%s`

			res=`cat /sys/module/dmatest/parameters/bw_mbps_total`
			log "Test took $((end-start)) sec. lmbench in the background $IS_LMBENCH"
			log "L3:$L3_LOCKING iter:$ITER buf_size:$BUF_SIZE sg_buffers:$SG_BUFFERS threads_per_chan:$THREADS_PER_CHAN => BW $res Mbps\n"
		done
	done
	[[ $IS_LMBENCH == yes ]] && stop_lmbench
done

#rmmod dmatest
log "#############################################"
log "Test ended `date`"
log "#############################################"
