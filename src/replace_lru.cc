#include "replace_lru.h"

#include <ctime>
namespace replace {

LRU_Replace::LRU_Replace (TraceHandle* th, uint32_t mem_size) {
    mTraceHandle = th;
    // initial PageTable
    mPageTable.resize (mTraceHandle->mVmSize + 1);
    for (uint32_t i = 0; i <= mTraceHandle->mVmSize; i++) {
        mPageTable[i].pf_times = 0;

        mPageTable[i].page_num = i;
        mPageTable[i].isResident = 0;

        mPageTable[i].LRU_next = NULL;
        mPageTable[i].LRU_prev = NULL;

        mPageTable[i].recency = mTraceHandle->mVmSize + 10;
    }

    // initial Cache
    mCacheMaxLimit = mem_size;
    mPageFaultNum = 0;
    mFreeMemSize = mem_size;
    ColdMiss = 0;

    LRU_list_head = NULL;
    LRU_list_tail = NULL;

    mName = "LRU";
}

void LRU_Replace::Run () {
    int64_t total_ref_pg = 0;
    int64_t last_ref_pg = -1;
    int64_t num = 0;
    uint32_t last_page_fault_num = 0;

    page_struct* temp_LRU_ptr;
    std::string filename;
    auto n = mTraceHandle->mFileName.find ("w");
    filename = mTraceHandle->mFileName.substr (n);

    uint32_t step = mTraceHandle->mTraceLength / 20;
    for (uint32_t i = 0; i < mTraceHandle->mTraceLength; i += step) printf (".");
    printf ("\n");

    time_t c_start, c_end;
    c_start = clock ();
    uint32_t ref_page = 0;
    for (uint32_t cur_ref = 0; cur_ref < mTraceHandle->mTraceLength; cur_ref++) {
        if (cur_ref % step == 0) {
            printf (".");
            fflush (NULL);
        }
        ref_page = mTraceHandle->mTrace[cur_ref];

        /* if the ref page not in the cache, MISS happend*/
        if (!mPageTable[ref_page].isResident) { /* page fault */
            mPageTable[ref_page].pf_times++;
            mPageFaultNum++;

            /* if cache size not enough, to free the block with larget receny */
            if (mFreeMemSize == 0) { /* free the LRU page */
                LRU_list_tail->isResident = 0;
                temp_LRU_ptr = LRU_list_tail;
                LRU_list_tail = LRU_list_tail->LRU_prev;
                LRU_list_tail->LRU_next = NULL;
                temp_LRU_ptr->LRU_prev = NULL;
                temp_LRU_ptr->LRU_next = NULL;
                mFreeMemSize++;
            }
            mPageTable[ref_page].isResident = 1;
            mFreeMemSize--;
        } else { /* hit in memroy */
            if (mPageTable[ref_page].LRU_prev)
                mPageTable[ref_page].LRU_prev->LRU_next = mPageTable[ref_page].LRU_next;
            else /* hit at the stack top */
                LRU_list_head = mPageTable[ref_page].LRU_next;

            if (mPageTable[ref_page].LRU_next)
                mPageTable[ref_page].LRU_next->LRU_prev = mPageTable[ref_page].LRU_prev;
            else /* hit at the stack bottom */
                LRU_list_tail = mPageTable[ref_page].LRU_prev;
        }

        mPageTable[ref_page].LRU_next = LRU_list_head; /* place newly referenced page at head */

        mPageTable[ref_page].LRU_prev = NULL;

        LRU_list_head = (page_struct*)&mPageTable[ref_page];

        if (LRU_list_head->LRU_next) LRU_list_head->LRU_next->LRU_prev = LRU_list_head;

        if (!LRU_list_tail) LRU_list_tail = LRU_list_head;
    }
    printf ("\n");
}

}  // namespace replace