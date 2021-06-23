#include "replace_lirs2.h"

#include <gflags/gflags.h>

#include <iostream>
#include <sstream>

#include "trace_handle.h"

using GFLAGS_NAMESPACE::ParseCommandLineFlags;
using GFLAGS_NAMESPACE::RegisterFlagValidator;
using GFLAGS_NAMESPACE::SetUsageMessage;

namespace replace {

LIRS2_Replace::LIRS2_Replace (TraceHandle* th, uint32_t mem_size) {
    mTraceHandle = th;
    /* initialize the page table */
    mPageTable.resize (mTraceHandle->mVmSize + 1);
    mPageTable1.resize (mTraceHandle->mVmSize + 1);
    mPageTable2.resize (mTraceHandle->mVmSize + 1);
    for (uint32_t i = 0; i <= mTraceHandle->mVmSize; i++) {
        mPageTable[i].page_num = i;
        mPageTable[i].isResident = false;
        mPageTable[i].isHIR_block = true;

        mPageTable1[i].page_num = i;
        mPageTable1[i].LIRS_next = NULL;
        mPageTable1[i].LIRS_prev = NULL;
        mPageTable1[i].HIR_rsd_next = NULL;
        mPageTable1[i].HIR_rsd_prev = NULL;
        mPageTable1[i].fake_ins = true;
        mPageTable1[i].which_instance = 0;
        // Used to mark comparison of recency and Smaxf
        mPageTable1[i].recency1 = S_STACK_OUT;
        mPageTable1[i].recency2 = S_STACK_OUT;

        mPageTable2[i].page_num = i;
        mPageTable2[i].LIRS_next = NULL;
        mPageTable2[i].LIRS_prev = NULL;
        mPageTable2[i].HIR_rsd_next = NULL;
        mPageTable2[i].HIR_rsd_prev = NULL;
        mPageTable2[i].fake_ins = true;
        mPageTable2[i].which_instance = 0;
        // Used to mark comparison of recency and Smax
        mPageTable2[i].recency1 = S_STACK_OUT;
        mPageTable2[i].recency2 = S_STACK_OUT;
    }

    // initial Cache
    mCacheMaxLimit = mem_size;
    mPageFaultNum = 0;
    mFreeMemSize = mem_size;
    mName = "LIRS2";

    EVICT_LIST_SIZE = 10;
    LOWEST_HG_NUM = 4;
    HIR_RATE = 1;
    MAX_S_LEN = mem_size * 8;

    PRE_REFER_FLAG = 0;

    cur_ins1_rmax1_len = 0;
    cur_ins2_rmax2_len = 0;
    cur_lir_S_len = 0;

    char path1[128];
    auto n = mTraceHandle->mFileName.find ("w");
    filename = mTraceHandle->mFileName.substr (n);

    /* Global var initial */
    LRU_list_head = NULL;
    LRU_list_tail = NULL;

    HIR_list_head = NULL;
    HIR_list_tail = NULL;

    Rmax1 = NULL;
    Rmax2 = NULL;

    evict_cur_idx = 0;
    evict_max_idx = EVICT_LIST_SIZE;

    /* the memory for hirs is 1% of memory size */
    HIR_block_portion_limit = (unsigned long)(HIR_RATE / 100.0 * mCacheMaxLimit);
    if (HIR_block_portion_limit < LOWEST_HG_NUM) HIR_block_portion_limit = LOWEST_HG_NUM;
}

void LIRS2_Replace::recordEvict (uint32_t page_num) { evict_list.push_back (page_num); }

/* Remove block from Stack Q */
bool LIRS2_Replace::removeHIRList (page_struct* HIR_block_ptr) {
    assert (HIR_block_ptr);

    // If prev and next pointers are not exist -> Del current node, now Stack Q is
    // empty
    if (HIR_block_ptr->HIR_rsd_next == NULL && HIR_block_ptr->HIR_rsd_prev == NULL) {
        assert (HIR_list_head == HIR_block_ptr && HIR_list_tail == HIR_block_ptr);
        HIR_list_head = HIR_list_tail = NULL;
    } else if (HIR_block_ptr->HIR_rsd_prev &&
               HIR_block_ptr->HIR_rsd_next) {  // If prev and next pointers are
                                               // exist -> Del current node
        HIR_block_ptr->HIR_rsd_prev->HIR_rsd_next = HIR_block_ptr->HIR_rsd_next;
        HIR_block_ptr->HIR_rsd_next->HIR_rsd_prev = HIR_block_ptr->HIR_rsd_prev;
        HIR_block_ptr->HIR_rsd_prev = HIR_block_ptr->HIR_rsd_next = NULL;
    } else if (!HIR_block_ptr->HIR_rsd_prev) {  // If prev pointer is not exists ->
                                                // the head of stack Q
        assert (HIR_block_ptr == HIR_list_head);
        HIR_list_head = HIR_block_ptr->HIR_rsd_next;
        HIR_list_head->HIR_rsd_prev->HIR_rsd_next = NULL;
        HIR_list_head->HIR_rsd_prev = NULL;
    } else if (!HIR_block_ptr->HIR_rsd_next) {  // If next pointer is not exists ->
                                                // the tail of stack Q
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

/* Find new Rmax1 or Rmax2 */
page_struct* LIRS2_Replace::findLastLirLru (uint32_t which_instance) {
    assert (which_instance == 1 || which_instance == 2);

    if (which_instance == 1) {
        assert (Rmax1 != NULL);
        while (Rmax1) {
            Rmax1->recency1 = S_STACK_OUT;
            if (Rmax1->which_instance == 1) cur_ins1_rmax1_len--;
            Rmax1 = Rmax1->LIRS_prev;
            if (!mPageTable[Rmax1->page_num].isHIR_block && Rmax1->which_instance == 1) {
                return Rmax1;
            }
        }
        printf ("Can't find the rmax1... \n");
        exit (1);
    }

    if (which_instance == 2) {
        assert (Rmax2 != NULL);
        while (Rmax2) {
            Rmax2->recency2 = S_STACK_OUT;
            if (Rmax2->which_instance == 2) cur_ins2_rmax2_len--;
            cur_lir_S_len--;
            Rmax2 = Rmax2->LIRS_prev;
            if (!mPageTable[Rmax2->page_num].isHIR_block && Rmax2->which_instance == 2) {
                return Rmax2;
            }
        }
        printf ("Can't find the rmax2... \n");
        exit (1);
    }
    return NULL;
}

/* remove the node from stack S */
bool LIRS2_Replace::removeLIRSList (page_struct* page_ptr) {
    assert (page_ptr);

    assert (page_ptr->LIRS_prev || page_ptr->LIRS_next);

    // If prev and next pointers are exist -> Del current node
    if (page_ptr->LIRS_prev && page_ptr->LIRS_next) {
        page_ptr->LIRS_prev->LIRS_next = page_ptr->LIRS_next;
        page_ptr->LIRS_next->LIRS_prev = page_ptr->LIRS_prev;
        page_ptr->LIRS_prev = page_ptr->LIRS_next = NULL;
    } else if (!page_ptr->LIRS_prev) {  // If prev pointer is not exists -> the
                                        // head of stack S
        LRU_list_head = page_ptr->LIRS_next;
        LRU_list_head->LIRS_prev->LIRS_next = NULL;
        LRU_list_head->LIRS_prev = NULL;
    } else if (!page_ptr->LIRS_next) {  // If next pointer is not exists -> the
                                        // tail of stack S
        LRU_list_tail = page_ptr->LIRS_prev;
        LRU_list_tail->LIRS_next->LIRS_prev = NULL;
        LRU_list_tail->LIRS_next = NULL;
    } else {
        perror ("Stack S remove Error \n");
        exit (0);
    }

    return true;
}

// Add node to stack S
void LIRS2_Replace::addLruListHead (page_struct* new_ref_ptr) {
    assert (new_ref_ptr);
    assert (!new_ref_ptr->LIRS_prev && !new_ref_ptr->LIRS_next);

    // If stack S is empty -> initial
    if (!LRU_list_head)
        LRU_list_head = LRU_list_tail = new_ref_ptr;
    else {
        new_ref_ptr->LIRS_next = LRU_list_head;
        LRU_list_head->LIRS_prev = new_ref_ptr;
        LRU_list_head = new_ref_ptr;
    }

    return;
}

/* put a HIR resident block on the end of HIR resident list */
void LIRS2_Replace::addHirlistHead (page_struct* new_rsd_HIR_ptr) {
    assert (new_rsd_HIR_ptr);

    if (!HIR_list_head)
        HIR_list_tail = HIR_list_head = new_rsd_HIR_ptr;
    else {
        new_rsd_HIR_ptr->HIR_rsd_next = HIR_list_head;
        HIR_list_head->HIR_rsd_prev = new_rsd_HIR_ptr;
        HIR_list_head = new_rsd_HIR_ptr;
    }

    return;
}

/* To address an extreme case, in which the size of LIR stack
 * is larger than the limitation(MAX_S_LEN).
 * Basic is find a HIR block start from Rmax2, and move it out of the stack.
 */
page_struct* LIRS2_Replace::pruneLIRSstack () {
    page_struct* tmp_ptr;
    while (cur_lir_S_len > MAX_S_LEN) {
        tmp_ptr = Rmax2;
        while (!mPageTable[tmp_ptr->page_num].isHIR_block) tmp_ptr = tmp_ptr->LIRS_prev;
        tmp_ptr->recency1 = S_STACK_OUT;
        tmp_ptr->recency2 = S_STACK_OUT;
        removeLIRSList (tmp_ptr);
        insertLRUList (tmp_ptr, Rmax2);
        cur_lir_S_len--;
    }
    return NULL;
}

/* insert a block into LIRS list */
void LIRS2_Replace::insertLRUList (page_struct* old_ref_ptr, page_struct* new_ref_ptr) {
    old_ref_ptr->LIRS_next = new_ref_ptr->LIRS_next;
    old_ref_ptr->LIRS_prev = new_ref_ptr;

    if (new_ref_ptr->LIRS_next) new_ref_ptr->LIRS_next->LIRS_prev = old_ref_ptr;
    new_ref_ptr->LIRS_next = old_ref_ptr;
    return;
}

// Find which instance of this block in stack Q
page_struct* LIRS2_Replace::findInstanceInQ (uint32_t block_number) {
    page_struct* instance = NULL;
    if (mPageTable1[block_number].HIR_rsd_prev || mPageTable1[block_number].HIR_rsd_next) {
        assert (!mPageTable2[block_number].HIR_rsd_prev && !mPageTable2[block_number].HIR_rsd_next);
        instance = &mPageTable1[block_number];
    }

    if (mPageTable2[block_number].HIR_rsd_prev || mPageTable2[block_number].HIR_rsd_next) {
        assert (!mPageTable1[block_number].HIR_rsd_prev && !mPageTable1[block_number].HIR_rsd_next);
        instance = &mPageTable2[block_number];
    }

    if (!mPageTable1[block_number].HIR_rsd_prev && !mPageTable1[block_number].HIR_rsd_next &&
        !mPageTable2[block_number].HIR_rsd_prev && !mPageTable2[block_number].HIR_rsd_next) {
        assert ((HIR_list_head == HIR_list_tail) && HIR_list_head && HIR_list_tail);
        instance = HIR_list_head;
    }
    // assert(instance);
    return instance;
}

// Find the instance of this block (instance 1 or instance 2)
page_struct* LIRS2_Replace::pointerInstance (uint32_t block_number, uint32_t which_instance) {
    if (mPageTable1[block_number].which_instance == which_instance) {
        assert (mPageTable2[block_number].which_instance != which_instance);
        return &mPageTable1[block_number];
    }

    if (mPageTable2[block_number].which_instance == which_instance) {
        assert (mPageTable1[block_number].which_instance != which_instance);
        return &mPageTable2[block_number];
    }
    // fflush(NULL);
    return NULL;
}

void LIRS2_Replace::Report () { Replace::Report (); }

void LIRS2_Replace::Run () {
    int64_t total_ref_pg = 0;
    int64_t last_ref_pg = -1;
    int64_t warm_pg_refs = 0;
    int64_t no_dup_refs = 0;
    int64_t num_LIR_pgs = 0;
    page_struct* ins1 = NULL;
    page_struct* ins2 = NULL;
    uint32_t ref_page;
    uint32_t step = mTraceHandle->mTraceLength / 20;

    for (uint32_t i = 0; i < mTraceHandle->mTraceLength; i += step) printf (".");
    printf ("\n");

    for (uint32_t cur_ref = 0; cur_ref < mTraceHandle->mTraceLength; cur_ref++) {
        if (cur_ref % step == 0) {
            printf (".");
            fflush (NULL);
        }

        ref_page = mTraceHandle->mTrace[cur_ref];

        if (ref_page == last_ref_pg) {
            continue;
        }
        last_ref_pg = ref_page;

        // Handle NON-RESIDENT HIR, RESIDENT HIR, LIR
        if (!mPageTable[ref_page].isResident) { /* Block miss, not resident in cache. Type including
                                                   NON-RESIDENT HIR && INIT LIR blocks */
            mPageFaultNum++;
            /* Not enough memory */
            if (mFreeMemSize == 0) {
                assert (HIR_list_tail);
                mPageTable[HIR_list_tail->page_num].isResident = false;
                removeHIRList (HIR_list_tail);
                mFreeMemSize++;
            } else if (mFreeMemSize > HIR_block_portion_limit) {
                mPageTable[ref_page].isHIR_block = false;
                num_LIR_pgs++;
            }
            mFreeMemSize--;
        } else if (mPageTable[ref_page].isHIR_block) { /* Resident in cache and it
                                                          is a HIR block. */
            /* RESIDENT HIR -> HIT, recency2, promote */
            removeHIRList (findInstanceInQ (ref_page));
        }

        ins1 = pointerInstance (ref_page, 1);
        ins2 = pointerInstance (ref_page, 2);

        if (!ins1 && !ins2) {
            // mPageTable[ref_page].reuse_distance.push_back(2*mCacheMaxLimit+1);

            mPageTable1[ref_page].which_instance = 1;
            mPageTable1[ref_page].recency1 = S_STACK_IN;
            mPageTable1[ref_page].recency2 = S_STACK_IN;
            mPageTable1[ref_page].fake_ins = false;

            mPageTable2[ref_page].which_instance = 2;
            mPageTable2[ref_page].recency1 = S_STACK_IN;
            mPageTable2[ref_page].recency2 = S_STACK_IN;
            mPageTable2[ref_page].fake_ins = true;

            addLruListHead (&mPageTable2[ref_page]);
            if (!Rmax2) {
                mPageTable2[ref_page].recency1 = S_STACK_OUT;
                Rmax2 = &mPageTable2[ref_page];
            }
            addLruListHead (&mPageTable1[ref_page]);
            if (!Rmax1) Rmax1 = &mPageTable1[ref_page];
            cur_ins1_rmax1_len++;
            cur_ins2_rmax2_len++;
            cur_lir_S_len += 2;

            if (mPageTable[ref_page].isHIR_block) {
                addHirlistHead (&mPageTable1[ref_page]);
            }
            mPageTable[ref_page].isResident = 1;
            pruneLIRSstack ();
            continue;
        }

        assert (ins1 && ins2);

        uint32_t ins1_recency1 = ins1->recency1;
        uint32_t ins2_recency2 = ins2->recency2;
        bool ins2_fake_ins = ins2->fake_ins;

        ins1->which_instance = 2;

        if (ins1->recency2 == S_STACK_IN) {
            cur_ins2_rmax2_len++;
        }

        ins2->which_instance = 1;
        if (ins2 == Rmax2) {
            findLastLirLru (2);
        } else {
            if (ins2->recency2 == S_STACK_IN) {
                cur_ins2_rmax2_len--;
                cur_lir_S_len--;
            }
        }

        if (ins2->fake_ins) ins2->fake_ins = false;

        removeLIRSList (ins2);
        addLruListHead (ins2);
        // move this Rmax1 adjustment operation down here to handle the case where
        // Rmax1 is at the stack top
        if (ins1 == Rmax1) {
            findLastLirLru (1);
        } else {
            if (ins1->recency1 == S_STACK_IN) {
                cur_ins1_rmax1_len--;
            }
        }
        // have to del the ins2 first then add them back to the top
        cur_ins1_rmax1_len++;
        cur_lir_S_len++;

        // fake instance2 indicates a block that has been accessed only once, but
        // has two instances in the stack
        if (mPageTable[ins2->page_num].isHIR_block && ins2_recency2 == S_STACK_IN &&
            !ins2_fake_ins) {
            mPageTable[ins2->page_num].isHIR_block = false;
            num_LIR_pgs++;

            assert (num_LIR_pgs > mCacheMaxLimit - HIR_block_portion_limit);
            mPageTable[Rmax2->page_num].isHIR_block = true;

            addHirlistHead (Rmax2);

            HIR_list_head->recency2 = S_STACK_OUT;
            num_LIR_pgs--;
            if (mPageTable[Rmax1->page_num].isHIR_block) {
                Rmax1 = findLastLirLru (1);
            }
            Rmax2 = findLastLirLru (2);
        } else if (mPageTable[ins2->page_num].isHIR_block) {
            addHirlistHead (ins2);
        }
        mPageTable[ref_page].isResident = 1;
        ins2->recency1 = S_STACK_IN;
        ins2->recency2 = S_STACK_IN;

        pruneLIRSstack ();
    }

    std::vector<base_info> (mPageTable).swap (mPageTable);
    std::vector<page_struct> (mPageTable1).swap (mPageTable1);
    std::vector<page_struct> (mPageTable2).swap (mPageTable2);
    return;
}

}  // namespace replace