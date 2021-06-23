# Descriptions:

   ./traces/{w2_pools, wcs, wcpp, wgli, wps, wsprite, wmulti1, wmulti2, wmulti3} are the trace files from LIRS. (String format)

   ./traces/{w001, w002, ..., w106} : 106 week-long virtual disk traces collected by CloudPhysics’s caching analytics service in production VMware environments. (Binary format)

   ./traces/{wmsr_proj, wmsr_src1, wmsr_src2, wmsr_web1} : 4 week-long enterprise server traces collected by Microsoft Research Cambridge. (Binary format)

   ./traces/{wFinancial1, wFinancial2, websearch1, websearch2, websearch3} : 5 I/O traces from the UMass Trace Repository. (String format)

   ./traces/{w110, w111} : SCAN and Zigzag pattern respectively.

   The trace just an array of unsigned int values -- LBN (logical block number)

# Install boost

sudo apt install libboost-all-dev

# How to run the program:

   The args of program:
   --trace : which trace you want to run
   --trace_folder : folder store trace parameter
   --readbinary : read file in binary mode
   --mem_size : cache size
   --method : using which replace algorithm(lirs2, lru, opt, lirs, arc)
   --batch : select false if there is a specific trace you want to run
   --trace_list : file that defines the trace list (--batch set to true)


   Follow the instructions below to run the program:
   mkdir build
   cd build
   cmake ..
   make -j32

   Running a single trace with string format
   ./replace --method=lirs2 --trace=w111

   Running a single trace with binary format
   ./replace --method=lirs2 --trace=w106  --readbinary=true

   Running multiple traces with string format (must contain list.txt in the folder)
   ./replace --method=lirs2 --batch=true --trace_list="list.txt"

   Running multiple methods including "lirs", "lirs2", "lru", "opt", "arc" (must contain list.txt in the folder)
   ./replace --batch=true --trace_list="list.txt"