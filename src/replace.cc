#include "replace.h"

#include "trace_handle.h"

namespace replace {

void Replace::Report () {
    FILE* write_ptr1;
    FILE* write_ptr2;
    char path1[128];
    char path2[128];
    char shortn[9];

    std::string filename = mTraceHandle->mFileName;
    std::string filePath = mTraceHandle->mFileName;
    std::string delimiter = "/";

    size_t pos = 0;
    std::string token;
    while ((pos = filename.find (delimiter)) != std::string::npos) {
        token = filename.substr (0, pos);
        filename.erase (0, pos + delimiter.length ());
    }
    printf ("filename = %s \n", filename.c_str ());

    if (mkdir ("../result_set/", 0777) != 0) {
        // perror ("cannot create result folder\n");
    }

    if (mkdir (("../result_set/" + filename).c_str (), 0777) != 0) {
        // perror ("cannot create trace result folder\n");
    }

    sprintf (path1, ("../result_set/" + filename + "/%s-%s").c_str (), filename.c_str (),
             mName.c_str ());

    char buffer[256];
    write_ptr1 = fopen (path1, "a");

    double missRatio1 = (double)mPageFaultNum / mTraceHandle->mTraceLength * 100.0;

    fprintf (write_ptr1, "%u,%2.1f,%2.1f\n", mCacheMaxLimit, missRatio1, 100 - missRatio1);

    fflush (write_ptr1);
    fclose (write_ptr1);

    printf ("========== %s %s ==========\n", mName.c_str (), mTraceHandle->mFileName.c_str ());
    printf ("cache_mem = %u\n", mCacheMaxLimit);
    printf ("total_ref_pg = %5u    mem_size = %u\n", mTraceHandle->mTraceLength, mCacheMaxLimit);
    printf ("num_pg_flt   = %5u    pf ratio = %2.1f%% \n", mPageFaultNum, missRatio1);
    fflush (NULL);
}

}  // namespace replace