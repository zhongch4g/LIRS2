#include <gflags/gflags.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <set>
#include <vector>

#include "replace.h"
#include "replace_arc.h"
#include "replace_lirs2.h"
#include "replace_lirs2_optimize.h"
#include "replace_lirs_base.h"
#include "replace_lru.h"
#include "replace_opt.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "threadpool.hpp"
using namespace boost::threadpool;

using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

#include <mutex>
// manualy set trace_folder/trace
DEFINE_string (trace, "Zigzag", "which trace you want to run");
DEFINE_string (trace_folder, "../traces/", "where the trace store");
DEFINE_string (
    parameter_folder, "../trace_parameter/",
    "folder store trace parameter, file name requires to keep the same as the trace file");
DEFINE_bool (readbinary, false, "read file in binary mode");
DEFINE_int64 (mem_size, 0, "mem size, run batch if mem_size = 0");
DEFINE_string (method, "lirs2", "using which replace algorithm(lru, opt, lirs, arc)");
DEFINE_bool (batch, false, "select false if there is a specific trace you want to run");
DEFINE_string (trace_list, "", "path of the trace list. For example ./list.txt");
DEFINE_int32 (pool, 1, "thread pool size");
DEFINE_int32 (cutline, -1, "");

using namespace replace;

std::mutex kReportLock;
class ReplaceJob {
public:
    ReplaceJob (std::string method, std::shared_ptr<TraceHandle> traceHandle,
                std::vector<uint64_t> mem_sizes)
        : mMethod (method), mTraceHandle (traceHandle), mMemSizes (mem_sizes) {}

    ReplaceJob (std::string method, std::shared_ptr<TraceHandle> traceHandle, uint64_t mem_size)
        : mMethod (method), mTraceHandle (traceHandle) {
        mMemSizes.push_back (mem_size);
    }
    void Run () {
        printf ("======================== job start ======================\n");
        for (auto mem_size : mMemSizes) {
            if (mMethod == "lru") {
                LRU_Replace lruReplace (mTraceHandle.get (), mem_size);
                lruReplace.Run ();
                kReportLock.lock ();
                lruReplace.Report ();
                kReportLock.unlock ();
            } else if (mMethod == "opt") {
                OPT_Replace optReplace (mTraceHandle.get (), mem_size);
                optReplace.Run ();
                kReportLock.lock ();
                optReplace.Report ();
                kReportLock.unlock ();
            } else if (mMethod == "lirs") {
                LIRS_Replace lirsReplace (mTraceHandle.get (), mem_size);
                lirsReplace.Run ();
                kReportLock.lock ();
                lirsReplace.Report ();
                kReportLock.unlock ();
            } else if (mMethod == "arc") {
                ARC_Replace arcReplace (mTraceHandle.get (), mem_size);
                arcReplace.Run ();
                kReportLock.lock ();
                arcReplace.Report ();
                kReportLock.unlock ();
            } else if (mMethod == "lirs2") {
                LIRS_Replace* replace = new LIRS2_Replace (mTraceHandle.get (), mem_size);
                replace->Run ();
                kReportLock.lock ();
                replace->Report ();
                kReportLock.unlock ();
            } else if (mMethod == "lirs2_optimize") {
                LIRS2_Optimize_Replace* replace =
                    new LIRS2_Optimize_Replace (mTraceHandle.get (), mem_size);
                replace->Run ();
                kReportLock.lock ();
                replace->Report ();
                kReportLock.unlock ();
            } else {
                printf ("method not support\n");
            }
        }
    }

private:
    std::string mMethod;
    std::shared_ptr<TraceHandle> mTraceHandle;
    std::vector<uint64_t> mMemSizes;
};

int main (int argc, char* argv[]) {
    static std::vector<std::string> kMethods = {"lirs", "lirs2", "lru", "opt", "arc"};
    FILE *trace_fp, *cuv_fp, *para_fp;
    ParseCommandLineFlags (&argc, &argv, true);

    pool tp;
    tp.size_controller ().resize (FLAGS_pool);

    int opt;
    uint64_t single_mem = 0;
    char trc_file_name[100];
    char para_file_name[100];
    char cuv_file_name[100];
    printf ("start %s algorithm..\n", FLAGS_method.c_str ());

    // Batch
    if (FLAGS_batch) {
        printf ("Here is auto mode..\n");
        if (FLAGS_mem_size != 0) {
            perror ("Auto mode not allowed assign mem_size\n");
            exit (1);
        }

        // if (trace_checker.compare ("") != 0) {
        //     perror ("Auto mode not allowed assign specific trace.\n");
        //     exit (1);
        // }

        // Auto step 1: read trace list
        std::vector<TraceHandle*> traces;
        std::vector<std::string> trace_list;
        // using file defined traces from trace_list
        // TODO: read list of trace name from FLAGS_trace_list
        FILE* tf;
        if ((tf = fopen (FLAGS_trace_list.c_str (), "r")) == NULL) {
            perror (
                "\nCould not find trace list file. \nbatch running only support for the traces in "
                "the list.txt file.\nplease create list.txt file in current folder,\n\and write "
                "the traces and sep them with Return\n\n");
            exit (1);
        }

        fseek (tf, 0, SEEK_SET);
        int i = 0;
        char buffer[20] = "";
        while (!feof (tf)) {
            int res = fscanf (tf, "%s", buffer);
            if (res < 0) break;
            trace_list.push_back (buffer);
        }
        fclose (tf);

        if (trace_list.empty ()) {
            perror ("\nNo traces inside the list \n\n");
            exit (1);
        }

        // Auto step 2: decode parameter(cache size) from list to 2-dim vec
        std::vector<std::vector<uint64_t>> mem_sizes;
        std::vector<uint64_t> temp;
        for (uint64_t i = 0; i < trace_list.size (); i++) {
            char buffer[20];
            std::string path = FLAGS_parameter_folder.c_str () + trace_list[i];
            printf ("Read par %s\n", path.c_str ());

            FILE* lf;

            if ((lf = fopen (path.c_str (), "r")) == NULL) {
                perror ("\nCould not find the parameter file \n\n");
                exit (1);
            }

            fseek (lf, 0, SEEK_SET);
            while (!feof (lf)) {
                int res = fscanf (lf, "%s", buffer);
                if (res < 0) break;
                temp.push_back (atoi (buffer));
            }
            mem_sizes.push_back (temp);
            temp.clear ();
            fclose (lf);
        }
        printf ("End Read Par\n");
        fflush (NULL);

        if (mem_sizes.empty ()) {
            perror ("\nNo parameter inside the file \n\n");
            exit (1);
        }

        // Auto step 3: run replace algorithm
        for (uint64_t k = 0; k < trace_list.size (); k++) {
            std::shared_ptr<TraceHandle> traceHandle =
                std::make_shared<TraceHandle> (FLAGS_readbinary);
            // TraceHandle* traceHandle = new TraceHandle(FLAGS_readbinary);
            std::string filename = FLAGS_trace_folder + trace_list[k];

            // Read the traces. May take long time if there are too much trace files.
            printf ("Read file from %s\n", filename.c_str ());
            traceHandle->Init (filename, FLAGS_cutline);

            printf ("trace = %s, VM_Size = %u\n", trace_list[k].c_str (), traceHandle->mVmSize);

            // Read parameters that match traces
            std::vector<uint64_t> mem_size = mem_sizes[k];

            for (auto ms : mem_size) {
                if (FLAGS_method == "all") {
                    for (auto method : kMethods) {
                        boost::shared_ptr<ReplaceJob> job (
                            new ReplaceJob (method, traceHandle, ms));
                        schedule (tp, boost::bind (&ReplaceJob::Run, job));
                    }
                } else {
                    boost::shared_ptr<ReplaceJob> job (
                        new ReplaceJob (FLAGS_method, traceHandle, ms));
                    schedule (tp, boost::bind (&ReplaceJob::Run, job));
                }
            }
        }

        fflush (NULL);

    } else  // Single
    {
        printf ("Here is manual mode..\n");
        if (FLAGS_trace_list != "") {
            perror ("Manual mode not allowed assign trace list\n");
            exit (1);
        }
        std::vector<TraceHandle*> traces;
        std::vector<std::string> trace_list;
        std::shared_ptr<TraceHandle> traceHandle = std::make_shared<TraceHandle> (FLAGS_readbinary);
        std::string filename = FLAGS_trace_folder + FLAGS_trace;

        printf ("Read par %s\n", filename.c_str ());
        printf ("filename: %s\n", filename.c_str ());

        FILE* lf;

        if ((lf = fopen ((FLAGS_parameter_folder + FLAGS_trace).c_str (), "r")) == NULL) {
            perror ("\nCould not find the parameter file \n\n");
            exit (1);
        }

        fseek (lf, 0, SEEK_SET);
        char buffer[128];
        std::vector<uint64_t> mem_sizes;
        while (!feof (lf)) {
            int res = fscanf (lf, "%s", buffer);
            if (res < 0) break;
            mem_sizes.push_back (atoi (buffer));
        }
        fclose (lf);
        fflush (NULL);
        traceHandle->Init (filename, FLAGS_cutline);
        if (FLAGS_mem_size == 0) {
            ReplaceJob job (FLAGS_method, traceHandle, mem_sizes);
            job.Run ();
        } else {
            ReplaceJob job (FLAGS_method, traceHandle, FLAGS_mem_size);
            job.Run ();
        }
    }
    return 0;
}
