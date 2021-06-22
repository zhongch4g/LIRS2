#include "replace_lirs_base.h"

#include <ctime>
#include <iostream>
#include <sstream>

#include "trace_handle.h"
namespace replace {

LIRS_Replace::LIRS_Replace (TraceHandle* th, uint32_t mem_size) {
    mTraceHandle = th;

    /* initialize the page table */
    mPageTable.resize (mTraceHandle->mVmSize + 1);
    for (uint32_t i = 0; i <= mTraceHandle->mVmSize; i++) {
        // mPageTable[i].ref_times = 0;
        mPageTable[i].pf_times = 0;

        mPageTable[i].page_num = i;
        mPageTable[i].isResident = 0;
        mPageTable[i].isHIR_block = 1;

        mPageTable[i].LIRS_next = NULL;
        mPageTable[i].LIRS_prev = NULL;

        mPageTable[i].HIR_rsd_next = NULL;
        mPageTable[i].HIR_rsd_prev = NULL;
        // Used to mark comparison of recency and Smax, true if stack in.
        mPageTable[i].recency = false;
        mPageTable[i].refer_time = 0;
    }

    // initial Cache
    mCacheMaxLimit = mem_size;
    mPageFaultNum = 0;
    mFreeMemSize = mem_size;
    ColdMiss = 0;
    mName = "LIRS";

    EVICT_LIST_SIZE = 10;
    LOWEST_HG_NUM = 2;
    STAT_START_POINT = 0;
    HIR_RATE = 1.0;
    MAX_S_LEN = (mem_size * 2500);

    cur_lir_S_len = 0;
    PRE_REFER_FLAG = 0;

    /* Global var initial */
    LRU_list_head = NULL;
    LRU_list_tail = NULL;

    HIR_list_head = NULL;
    HIR_list_tail = NULL;

    LIR_LRU_block_ptr = NULL;

    evict_cur_idx = 0;
    evict_max_idx = EVICT_LIST_SIZE;

    /* the memory for hirs is 1% of memory size */
    HIR_block_portion_limit = (unsigned long)(HIR_RATE / 100.0 * mCacheMaxLimit);
    if (HIR_block_portion_limit < LOWEST_HG_NUM) HIR_block_portion_limit = LOWEST_HG_NUM;
}

void LIRS_Replace::recordEvict (uint32_t page_num) { evict_list.push_back (page_num); }

bool LIRS_Replace::removeHIRList (page_struct* HIR_block_ptr) {
    // If prev and next pointers are not exist -> Del current node, now Stack Q is empty
    if (HIR_block_ptr->HIR_rsd_next == NULL && HIR_block_ptr->HIR_rsd_prev == NULL) {
        assert (HIR_list_head == HIR_block_ptr && HIR_list_tail == HIR_block_ptr);
        HIR_list_head = HIR_list_tail = NULL;
    } else if (HIR_block_ptr->HIR_rsd_prev &&
               HIR_block_ptr
                   ->HIR_rsd_next) {  // If prev and next pointers are exist -> Del current node
        HIR_block_ptr->HIR_rsd_prev->HIR_rsd_next = HIR_block_ptr->HIR_rsd_next;
        HIR_block_ptr->HIR_rsd_next->HIR_rsd_prev = HIR_block_ptr->HIR_rsd_prev;
        HIR_block_ptr->HIR_rsd_prev = HIR_block_ptr->HIR_rsd_next = NULL;
    } else if (!HIR_block_ptr
                    ->HIR_rsd_prev) {  // If prev pointer is not exists -> the head of stack Q
        assert (HIR_block_ptr == HIR_list_head);
        HIR_list_head = HIR_block_ptr->HIR_rsd_next;
        HIR_list_head->HIR_rsd_prev->HIR_rsd_next = NULL;
        HIR_list_head->HIR_rsd_prev = NULL;
    } else if (!HIR_block_ptr
                    ->HIR_rsd_next) {  // If next pointer is not exists -> the tail of stack Q
        assert (HIR_block_ptr == HIR_list_tail);
        HIR_list_tail = HIR_block_ptr->HIR_rsd_prev;
        HIR_list_tail->HIR_rsd_next->HIR_rsd_prev = NULL;
        HIR_list_tail->HIR_rsd_next = NULL;
    } else {
        perror ("Stack Q remove Error \n");
        exit (0);
    }

    return true;
}

page_struct* LIRS_Replace::findLastLirLru () {
    if (!LIR_LRU_block_ptr) {
        printf ("Warning*\n");
        exit (1);
    }
    while (LIR_LRU_block_ptr->isHIR_block == true) {
        LIR_LRU_block_ptr->recency = false;
        cur_lir_S_len--;
        LIR_LRU_block_ptr = LIR_LRU_block_ptr->LIRS_prev;
    }
    return LIR_LRU_block_ptr;
}

bool LIRS_Replace::removeLIRSList (page_struct* page_ptr) {
    if (!page_ptr) return false;

    if (!page_ptr->LIRS_prev && !page_ptr->LIRS_next) return true;

    if (page_ptr == LIR_LRU_block_ptr) {
        LIR_LRU_block_ptr = page_ptr->LIRS_prev;
        LIR_LRU_block_ptr = findLastLirLru ();  // prune the LIR stack
    }

    if (!page_ptr->LIRS_prev)
        LRU_list_head = page_ptr->LIRS_next;
    else
        page_ptr->LIRS_prev->LIRS_next = page_ptr->LIRS_next;

    if (!page_ptr->LIRS_next)
        LRU_list_tail = page_ptr->LIRS_prev;
    else
        page_ptr->LIRS_next->LIRS_prev = page_ptr->LIRS_prev;

    page_ptr->LIRS_prev = page_ptr->LIRS_next = NULL;
    return true;
}

void LIRS_Replace::addLruListHead (page_struct* new_ref_ptr) {
    new_ref_ptr->LIRS_next = LRU_list_head;
    if (!LRU_list_head) {
        LRU_list_head = LRU_list_tail = new_ref_ptr;
        LIR_LRU_block_ptr = LRU_list_tail; /* since now the point to lir page with Smax isn't nil */
    } else {
        LRU_list_head->LIRS_prev = new_ref_ptr;
        LRU_list_head = new_ref_ptr;
    }
    return;
}

/* put a HIR resident block on the end of HIR resident list */
void LIRS_Replace::addHirlistHead (page_struct* new_rsd_HIR_ptr) {
    new_rsd_HIR_ptr->HIR_rsd_next = HIR_list_head;
    if (!HIR_list_head)
        HIR_list_tail = HIR_list_head = new_rsd_HIR_ptr;
    else
        HIR_list_head->HIR_rsd_prev = new_rsd_HIR_ptr;
    HIR_list_head = new_rsd_HIR_ptr;

    return;
}

page_struct* LIRS_Replace::pruneLIRSstack () {
    page_struct* tmp_ptr;
    int i = 0;

    if (cur_lir_S_len <= MAX_S_LEN) return NULL;

    tmp_ptr = LIR_LRU_block_ptr;
    while (tmp_ptr->isHIR_block == 0) tmp_ptr = tmp_ptr->LIRS_prev;

    tmp_ptr->recency = false;
    removeLIRSList (tmp_ptr);
    insertLRUList (tmp_ptr, LIR_LRU_block_ptr);
    cur_lir_S_len--;
    return tmp_ptr;
}

/* insert a block in LIRS list */
void LIRS_Replace::insertLRUList (page_struct* old_ref_ptr, page_struct* new_ref_ptr) {
    old_ref_ptr->LIRS_next = new_ref_ptr->LIRS_next;
    old_ref_ptr->LIRS_prev = new_ref_ptr;

    if (new_ref_ptr->LIRS_next) new_ref_ptr->LIRS_next->LIRS_prev = old_ref_ptr;
    new_ref_ptr->LIRS_next = old_ref_ptr;
    return;
}

void LIRS_Replace::Report () { Replace::Report (); }

void LIRS_Replace::Run () {
    int64_t total_ref_pg = 0;
    int64_t last_ref_pg = -1;
    int64_t warm_pg_refs = 0;
    int64_t collect_stat = (STAT_START_POINT == 0) ? 1 : 0;
    int64_t no_dup_refs = 0;
    int64_t num_LIR_pgs = 0;
    uint32_t pre_refer_time = 0;
    uint32_t in_stack_miss = 0;
    uint32_t out_stack_miss = 0;
    uint32_t initial_miss = 0;
    uint32_t last_seg_miss = 0;
    std::string filename;
    auto n = mTraceHandle->mFileName.find ("w");
    filename = mTraceHandle->mFileName.substr (n);

    uint32_t step = mTraceHandle->mTraceLength / 20;
    for (uint32_t i = 0; i < mTraceHandle->mTraceLength; i += step) printf (".");
    printf ("\n");

    time_t c_start, c_end;
    c_start = clock ();
    uint32_t ref_page;
    for (uint32_t cur_ref = 0; cur_ref < mTraceHandle->mTraceLength; cur_ref++) {
        if (cur_ref % step == 0) {
            printf (".");
            fflush (NULL);
        }
        ref_page = mTraceHandle->mTrace[cur_ref];

        // Remove duplicate
        if (ref_page == last_ref_pg) continue;
        last_ref_pg = ref_page;

        if (!mPageTable[ref_page].isResident) { /* block miss */
            mPageFaultNum++;
            /* not enough memory */
            if (mFreeMemSize == 0) {
                /* remove the "front" of the HIR resident page from cache (queue Q),
            but not from LIRS stack S */
                /* actually Q is an LRU stack, "front" is the bottom of the stack,
            "end" is its top */
                HIR_list_tail->isResident = 0;
                // recordEvict(HIR_list_tail->page_num);
                removeHIRList (HIR_list_tail);
                mFreeMemSize++;
            } else if (mFreeMemSize > HIR_block_portion_limit) {
                mPageTable[ref_page].isHIR_block = false;
                num_LIR_pgs++;
            }

            mFreeMemSize--;
        } else if (mPageTable[ref_page].isHIR_block) {
            removeHIRList (&mPageTable[ref_page]);
        }

        removeLIRSList (&mPageTable[ref_page]);

        /* place newly referenced page at head */
        addLruListHead (&mPageTable[ref_page]);

        mPageTable[ref_page].isResident = 1;
        if (mPageTable[ref_page].recency == false) cur_lir_S_len++;

        /* If newly reference block with HIR
        and Resident status, change it to LIR,
        if the S STACK length greater than its limit, pruning */
        if (mPageTable[ref_page].isHIR_block && (mPageTable[ref_page].recency == true)) {
            mPageTable[ref_page].isHIR_block = false;
            num_LIR_pgs++;

            if (num_LIR_pgs > mCacheMaxLimit - HIR_block_portion_limit) {
                addHirlistHead (LIR_LRU_block_ptr);
                HIR_list_head->isHIR_block = true;
                HIR_list_head->recency = false;
                num_LIR_pgs--;
                LIR_LRU_block_ptr = findLastLirLru ();  // prune the LIR stack
            } else {
                printf ("Warning2!\n");
            }
        }
        /* add page into Queue Q */
        else if (mPageTable[ref_page].isHIR_block) {
            addHirlistHead (&mPageTable[ref_page]);
        }

        mPageTable[ref_page].recency = true;

        // pruneLIRSstack();
    }
    printf ("\n");
    return;
}

}  // namespace replace