#include "replace_opt.h"

namespace replace {

OPT_Replace::OPT_Replace (TraceHandle* th, uint32_t mem_size) {
    mTraceHandle = th;
    // initial PageTable
    mPageTable.resize (mTraceHandle->mVmSize + 1);
    for (uint32_t i = 0; i <= mTraceHandle->mVmSize; i++) {
        mPageTable[i].fwd_distance = 0;
        mPageTable[i].isResident = 0;
    }

    // initial Cache
    mCacheMaxLimit = mem_size;
    mPageFaultNum = 0;
    mFreeMemSize = mem_size;
    ColdMiss = 0;
    mName = "OPT";
}
void OPT_Replace::showStack () {
    for (auto i : mPageTable) {
        std::string isres = std::to_string (i.isResident);
        printf ("%s ", (isres).c_str ());
    }
    printf ("\n");
    fflush (NULL);
}
void OPT_Replace::Run () {
    int64_t last_ref_pg = -1;
    int32_t last_page_fault_num = 0;
    uint32_t ref_page = 0;
    // start replace
    uint32_t step = mTraceHandle->mTraceLength / 20;

    for (uint32_t i = 0; i < mTraceHandle->mTraceLength; i += step) printf (".");
    printf ("\n");

    for (uint32_t cur_ref = 0; cur_ref < mTraceHandle->mTraceLength; cur_ref++) {
        if (cur_ref > 0 && cur_ref % step == 0) {
            printf (".");
            fflush (NULL);
        }
        ref_page = mTraceHandle->mTrace[cur_ref];

        if (mPageTable[ref_page].ref_times == 0) ColdMiss++;

        mPageTable[ref_page].ref_times++;

        if (ref_page > mTraceHandle->mVmSize) {
            printf ("Wrong ref page number found: %d.\n", ref_page);
            return;
        }

        if (!mPageTable[ref_page].isResident) { /* page fault */
            mPageFaultNum++;
            if (mFreeMemSize == 0) { /* free the LRU page */
                auto k = mCache.rbegin ();
                page_struct* obj = *k;
                obj->isResident = 0;
                mCache.erase (obj);
                mFreeMemSize++;
            }
            mPageTable[ref_page].isResident = 1;
            mFreeMemSize--;
        } else { /* hit in memroy */
            int res = mCache.erase (&mPageTable[ref_page]);
            if (res == 0) {
                perror ("erase fail\n");
                exit (1);
            }
        }

        // find next postion
        uint32_t nextPos = mTraceHandle->mNextPos[cur_ref];
        if (nextPos == 0) {
            nextPos = mTraceHandle->mTraceLength;
        }

        if (nextPos == mTraceHandle->mTraceLength)
            mPageTable[ref_page].fwd_distance = mTraceHandle->mTraceLength + cur_ref;
        else {
            mPageTable[ref_page].fwd_distance = nextPos + cur_ref;
        }

        mCache.insert (&mPageTable[ref_page]);
        if (mCache.size () > mCacheMaxLimit) {
            perror ("cache overflow\n");
            exit (0);
        }
    }
    printf ("\n");
}

}  // namespace replace