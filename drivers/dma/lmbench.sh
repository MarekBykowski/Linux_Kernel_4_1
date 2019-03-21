#!/bin/bash

#If lmbench binary doesn't exist go to cd $LMBENCH and run 'make'

LMBENCH=/workspace/sw/mbykowsx/lionfish/perf/nuevo/cpu/axm6732/asic_v1.1/lmbench3/test_l3_lockdown_issue
LMBENCH_PID=()

run_lmbench() {
	pushd .
	cd $LMBENCH
	export PATH=$LMBENCH/../bin/armv8l-linux-gnu:$PATH

	LM_SIZES=(
		1m 2m 3m 4m 5m 6m 7m 8m 9m 10m 11m
		12m 13m 14m 15m 16m 17m 1m 2m 3m
		1m 2m 3m 4m 5m 6m 7m 8m 9m 10m 11m
	)

	for ((cpu=0; cpu<=31; cpu++)); do
		echo spwaning LmBench on $cpu
		./l3_dma_run_bw_mem.sh $cpu ${LM_SIZES[$cpu]} &
		LMBENCH_PID+=($!)
		#echo 0.00$((RANDOM%20))
		sleep 0.00$((RANDOM%20))
	done
	popd
}


stop_lmbench() {
	#one can print what cpu lmbench assigned to 'ps -eo psr,comm'
	kill ${LMBENCH_PID[@]} > /dev/null 2>&1
	#for pid in "${LMBENCH_PID[@]}"; do
	#	kill $pid
	#done
}
