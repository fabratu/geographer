name=bcastbenchmrk
#@job_name         = MPI_code
#@output           = result_benchmark_16384cores_$(jobid)_$(stepid).out
#@error            = result_benchmark_16384cores_$(jobid)_$(stepid).err
#@environment      = COPY_ALL
#@job_type         = bluegene
#@notification     = always
#@notify_user      = michael.axtmann@kit.edu
#@bg_shape         = 1x1x1x2
#@bg_connectivity  = torus
#@wall_clock_limit = 0:10:00
#@queue

module list

runjob --ranks-per-node 16 : ~/ShizoQuicksort/build/benchmark 10 0 18 bcast bcastMPI bcast bcastMPI