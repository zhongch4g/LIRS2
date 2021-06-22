
#include "replace_arc.h"

namespace replace {

ARC_Replace::ARC_Replace (TraceHandle* th, uint32_t mem_size) {
    mTraceHandle = th;

    /* initialize the page table */
    mPageTable.resize (mTraceHandle->mVmSize + 1);
    /* initialize the page table */
    for (uint32_t i = 0; i <= mTraceHandle->mVmSize; i++) {
        mPageTable[i].ref_times = 0;
        mPageTable[i].pf_times = 0;

        mPageTable[i].page_num = i;
        mPageTable[i].isResident = 0;

        mPageTable[i].block_type = NO_BLK;

        mPageTable[i].next = NULL;
        mPageTable[i].prev = NULL;
    }

    T1_list_head = NULL;
    T1_list_tail = NULL;
    B1_list_head = NULL;
    B1_list_tail = NULL;

    T2_list_head = NULL;
    T2_list_tail = NULL;
    B2_list_head = NULL;
    B2_list_tail = NULL;

    // initial Cache
    mCacheMaxLimit = mem_size;
    mPageFaultNum = 0;
    mFreeMemSize = mem_size;
    mName = "ARC";

    T1_size = T2_size = B1_size = B2_size = 0;
    STAT_START_POINT = 0;
}

ARC_Replace::page_struct* ARC_Replace::move_from_T1 (ARC_Replace::page_struct* ptr) {
    if (ptr == NULL) {
        perror ("ptr Error\n");
        exit (1);
    }
    if (ptr->block_type != T1_BLK) {
        perror ("ptr Error\n");
        exit (1);
    }

    ptr->isResident = false;

    if (ptr->prev == NULL) {
        /*???if (ptr != T1_list_head) {
          printf("ptr->page_num = %d\n", ptr->page_num);
          exit(1);
          }*/
        if (ptr != T1_list_head) {
            perror ("ptr Error\n");
            exit (1);
        }
        T1_list_head = T1_list_head->next;
        if (!T1_list_head)
            T1_list_tail = NULL;
        else
            T1_list_head->prev = NULL;
        ptr->next = ptr->prev = NULL;
        T1_size--;
        ptr->block_type = NO_BLK;
        return ptr;
        ;
    }

    if (ptr->next == NULL) {
        if (ptr != T1_list_tail) {
            perror ("ptr Error\n");
            exit (1);
        }
        del_T1_LRU ();
        return ptr;
    }

    ptr->prev->next = ptr->next;
    ptr->next->prev = ptr->prev;
    ptr->next = ptr->prev = NULL;
    T1_size--;
    ptr->block_type = NO_BLK;

    return ptr;
}

ARC_Replace::page_struct* ARC_Replace::move_from_T2 (ARC_Replace::page_struct* ptr) {
    if (ptr == NULL) {
        perror ("ptr Error\n");
        exit (1);
    }
    if (ptr->block_type != T2_BLK) {
        perror ("ptr type Error\n");
        exit (1);
    }

    ptr->isResident = false;

    if (ptr->prev == NULL) {
        if (ptr != T2_list_head) {
            perror ("ptr Error\n");
            exit (1);
        }
        T2_list_head = T2_list_head->next;
        if (!T2_list_head)
            T2_list_tail = NULL;
        else
            T2_list_head->prev = NULL;
        ptr->next = ptr->prev = NULL;
        T2_size--;
        ptr->block_type = NO_BLK;
        return ptr;
        ;
    }

    if (ptr->next == NULL) {
        if (ptr != T2_list_tail) {
            perror ("ptr Error\n");
            exit (1);
        }
        del_T2_LRU ();
        return ptr;
    }

    ptr->prev->next = ptr->next;
    ptr->next->prev = ptr->prev;
    ptr->next = ptr->prev = NULL;
    T2_size--;
    ptr->block_type = NO_BLK;

    return ptr;
}

ARC_Replace::page_struct* ARC_Replace::del_T1_LRU () {
    struct pf_struct* ret_ptr;

    if (!T1_list_head) {
        printf ("Attemp to delete from empty T1!\n");
        return NULL;
    }

    T1_list_tail->block_type = NO_BLK;
    T1_list_tail->isResident = false;

    if (T1_list_head == T1_list_tail) {
        ret_ptr = T1_list_tail;
        T1_list_head = NULL;
        T1_list_tail = NULL;
        if (T1_size != 1) {
            perror ("T1_size Error\n");
            exit (1);
        }
        T1_size = 0;
        ret_ptr->next = ret_ptr->prev = NULL;
        return ret_ptr;
    }

    ret_ptr = T1_list_tail;
    T1_list_tail = T1_list_tail->prev;
    T1_list_tail->next = NULL;
    ret_ptr->next = ret_ptr->prev = NULL;

    T1_size--;

    return ret_ptr;
}

ARC_Replace::page_struct* ARC_Replace::del_T2_LRU () {
    struct pf_struct* ret_ptr;

    if (!T2_list_head) {
        printf ("Attemp to delete from empty T2!\n");
        return NULL;
    }

    T2_list_tail->block_type = NO_BLK;
    T2_list_tail->isResident = false;

    if (T2_list_head == T2_list_tail) {
        ret_ptr = T2_list_tail;
        T2_list_head = NULL;
        T2_list_tail = NULL;
        if (T2_size != 1) {
            perror ("T2_size Error\n");
            exit (1);
        }
        T2_size = 0;
        ret_ptr->next = ret_ptr->prev = NULL;
        return ret_ptr;
    }

    ret_ptr = T2_list_tail;
    T2_list_tail = T2_list_tail->prev;
    T2_list_tail->next = NULL;
    ret_ptr->next = ret_ptr->prev = NULL;

    T2_size--;

    return ret_ptr;
}

int ARC_Replace::add_T2_MRU (struct pf_struct* ptr) {
    if (ptr == NULL) {
        perror ("ptr Error\n");
        exit (1);
    }

    ptr->block_type = T2_BLK;
    ptr->isResident = true;

    if (!T2_list_head) {
        if (T2_list_tail != NULL) {
            perror ("T2 Error\n");
            exit (1);
        }
        if (T2_size != 0) {
            perror ("T2 size Error\n");
            exit (1);
        }

        T2_list_head = T2_list_tail = ptr;
        ptr->next = ptr->prev = NULL;
        T2_size = 1;
        return 1;
    }

    ptr->next = T2_list_head;
    ptr->prev = NULL;
    T2_list_head->prev = ptr;
    T2_list_head = ptr;

    T2_size++;

    return 1;
}

ARC_Replace::page_struct* ARC_Replace::move_from_B1 (ARC_Replace::page_struct* ptr) {
    if (ptr == NULL) {
        perror ("ptr Error\n");
        exit (1);
    }
    if (ptr->block_type != B1_BLK) {
        perror ("ptr Error\n");
        exit (1);
    }
    if (ptr->isResident != false) {
        perror ("ptr Error\n");
        exit (1);
    }

    if (ptr->prev == NULL) {
        if (ptr != B1_list_head) {
            perror ("ptr Error\n");
            exit (1);
        }
        B1_list_head = B1_list_head->next;
        if (!B1_list_head)
            B1_list_tail = NULL;
        else
            B1_list_head->prev = NULL;
        ptr->next = ptr->prev = NULL;
        B1_size--;
        ptr->block_type = NO_BLK;
        return ptr;
        ;
    }

    if (ptr->next == NULL) {
        if (ptr != B1_list_tail) {
            perror ("ptr Error\n");
            exit (1);
        }
        del_B1_LRU ();
        return ptr;
    }

    ptr->prev->next = ptr->next;
    ptr->next->prev = ptr->prev;
    ptr->next = ptr->prev = NULL;
    B1_size--;
    ptr->block_type = NO_BLK;

    return ptr;
}

ARC_Replace::page_struct* ARC_Replace::move_from_B2 (ARC_Replace::page_struct* ptr) {
    if (ptr == NULL) {
        perror ("ptr Error\n");
        exit (1);
    }
    if (ptr->block_type != B2_BLK) {
        perror ("ptr Error\n");
        exit (1);
    }
    if (ptr->isResident != false) {
        perror ("ptr Error\n");
        exit (1);
    }

    if (ptr->prev == NULL) {
        if (ptr != B2_list_head) {
            perror ("ptr Error\n");
            exit (1);
        }
        B2_list_head = B2_list_head->next;
        if (!B2_list_head)
            B2_list_tail = NULL;
        else
            B2_list_head->prev = NULL;
        ptr->next = ptr->prev = NULL;
        B2_size--;
        ptr->block_type = NO_BLK;
        return ptr;
        ;
    }

    if (ptr->next == NULL) {
        if (ptr != B2_list_tail) {
            perror ("ptr Error\n");
            exit (1);
        }
        del_B2_LRU ();
        return ptr;
    }

    ptr->prev->next = ptr->next;
    ptr->next->prev = ptr->prev;
    ptr->next = ptr->prev = NULL;
    B2_size--;
    ptr->block_type = NO_BLK;

    return ptr;
}

/* through the replacement to force the size of T1 around p */
int ARC_Replace::replace (page_struct* pg_ptr, int p) {
    page_struct* ptr;

    if ((T1_size > 0) && (T1_size > p || (pg_ptr->block_type == B2_BLK && T1_size == p))) {
        ptr = del_T1_LRU ();
        add_B1_MRU (ptr);
    } else {
        ptr = del_T2_LRU ();
        add_B2_MRU (ptr);
    }

    return 1;
}

int ARC_Replace::add_B1_MRU (struct pf_struct* ptr) {
    if (ptr == NULL) {
        perror ("ptr Error\n");
        exit (1);
    }

    ptr->block_type = B1_BLK;

    if (!B1_list_head) {
        if (B1_list_tail != NULL) {
            perror ("B1 Error\n");
            exit (1);
        }
        if (B1_size != 0) {
            perror ("B1 Error\n");
            exit (1);
        }

        B1_list_head = B1_list_tail = ptr;
        ptr->next = ptr->prev = NULL;
        B1_size = 1;
        return 1;
    }

    ptr->next = B1_list_head;
    ptr->prev = NULL;
    B1_list_head->prev = ptr;
    B1_list_head = ptr;

    B1_size++;

    return 1;
}

int ARC_Replace::add_B2_MRU (struct pf_struct* ptr) {
    if (ptr == NULL) {
        perror ("ptr Error\n");
        exit (1);
    }

    ptr->block_type = B2_BLK;

    if (!B2_list_head) {
        if (B2_list_tail != NULL) {
            perror ("B2 tail Error\n");
            exit (1);
        }
        if (B2_size != 0) {
            perror ("B2 size Error\n");
            exit (1);
        }

        B2_list_head = B2_list_tail = ptr;
        ptr->next = ptr->prev = NULL;
        B2_size = 1;
        return 1;
    }

    ptr->next = B2_list_head;
    ptr->prev = NULL;
    B2_list_head->prev = ptr;
    B2_list_head = ptr;

    B2_size++;

    return 1;
}

ARC_Replace::page_struct* ARC_Replace::del_B1_LRU () {
    struct pf_struct* ret_ptr;

    if (!B1_list_head) {
        printf ("Attemp to delete from empty B1!\n");
        return NULL;
    }

    B1_list_tail->block_type = NO_BLK;

    if (B1_list_head == B1_list_tail) {
        ret_ptr = B1_list_tail;
        B1_list_head = NULL;
        B1_list_tail = NULL;
        if (B1_size != 1) {
            perror ("B1 size Error\n");
            exit (1);
        }
        B1_size = 0;
        ret_ptr->next = ret_ptr->prev = NULL;
        return ret_ptr;
    }

    ret_ptr = B1_list_tail;
    B1_list_tail = B1_list_tail->prev;
    B1_list_tail->next = NULL;
    ret_ptr->next = ret_ptr->prev = NULL;

    B1_size--;

    return ret_ptr;
}

ARC_Replace::page_struct* ARC_Replace::del_B2_LRU () {
    struct pf_struct* ret_ptr;

    if (!B2_list_head) {
        printf ("Attemp to delete from empty B2!\n");
        return NULL;
    }

    B2_list_tail->block_type = NO_BLK;

    if (B2_list_head == B2_list_tail) {
        ret_ptr = B2_list_tail;
        B2_list_head = NULL;
        B2_list_tail = NULL;
        if (B2_size != 1) {
            perror ("B2 size Error\n");
            exit (1);
        }
        B2_size = 0;
        ret_ptr->next = ret_ptr->prev = NULL;
        return ret_ptr;
    }

    ret_ptr = B2_list_tail;
    B2_list_tail = B2_list_tail->prev;
    B2_list_tail->next = NULL;
    ret_ptr->next = ret_ptr->prev = NULL;

    B2_size--;

    return ret_ptr;
}

int ARC_Replace::add_T1_MRU (struct pf_struct* ptr) {
    if (ptr == NULL) {
        perror ("ptr Error\n");
        exit (1);
    }

    ptr->block_type = T1_BLK;
    ptr->isResident = true;

    if (!T1_list_head) {
        if (T1_list_tail != NULL) {
            perror ("Error\n");
            exit (1);
        }
        if (T1_size != 0) {
            perror ("Error\n");
            exit (1);
        }

        T1_list_head = T1_list_tail = ptr;
        ptr->next = ptr->prev = NULL;
        T1_size = 1;
        return 1;
    }

    ptr->next = T1_list_head;
    ptr->prev = NULL;
    T1_list_head->prev = ptr;
    T1_list_head = ptr;

    T1_size++;

    return 1;
}

uint32_t ARC_Replace::MIN (uint32_t a, uint32_t b) { return (a <= b) ? (a) : (b); }

void ARC_Replace::Run () {
    int64_t total_ref_pg = 0;
    int64_t warm_pg_refs = 0;
    int64_t last_ref_pg = -1;
    int64_t no_dup_refs = 0;
    int64_t collect_stat = (STAT_START_POINT == 0) ? 1 : 0;
    int64_t count = 0;
    /* adaptable tuning parameter */
    uint32_t p = 0;
    uint16_t delta;
    struct pf_struct* ptr;

    uint32_t step = mTraceHandle->mTraceLength / 20;
    for (uint32_t i = 0; i < mTraceHandle->mTraceLength; i += step) printf (".");
    printf ("\n");

    uint32_t ref_page = 0;
    for (uint32_t cur_ref = 0; cur_ref < mTraceHandle->mTraceLength; cur_ref++) {
        // if (cur_ref % step == 0)
        // {
        //     printf(".");
        //     fflush(NULL);
        // }
        ref_page = mTraceHandle->mTrace[cur_ref];
        // if (total_ref_pg % 10000 == 0)
        //     fprintf(stderr, "%llu samples processed\r", total_ref_pg);

        if (total_ref_pg > STAT_START_POINT) {
            collect_stat = 1;
            warm_pg_refs++;
        }

        if (ref_page > mTraceHandle->mVmSize) {
            printf ("Wrong ref page number found: %d.\n", ref_page);
            exit (0);
        }

        // if (ref_page == last_ref_pg)
        //     continue;
        // else
        //     last_ref_pg = ref_page;

        no_dup_refs++; /* ref counter excluding duplicate refs */

        /**** CASE I *******/
        if (mPageTable[ref_page].block_type == T1_BLK ||
            mPageTable[ref_page].block_type == T2_BLK) {
            if (mPageTable[ref_page].isResident != true) {
                perror ("CASE1 Error\n");
                exit (1);
            }
            if (mPageTable[ref_page].block_type == T1_BLK)
                ptr = move_from_T1 (&(mPageTable[ref_page]));
            else
                ptr = move_from_T2 (&(mPageTable[ref_page]));

            add_T2_MRU (ptr);
        }

        /**** CASE II *******/
        else if (mPageTable[ref_page].block_type == B1_BLK) {
            mPageFaultNum++;

            delta = (B1_size >= B2_size) ? 1 : B2_size / B1_size;
            p = MIN (p + delta, mCacheMaxLimit);

            replace (&mPageTable[ref_page], p);

            ptr = move_from_B1 (&mPageTable[ref_page]);
            add_T2_MRU (ptr);
        }

        /**** CASE III *******/
        else if (mPageTable[ref_page].block_type == B2_BLK) {
            mPageFaultNum++;

            delta = (B2_size >= B1_size) ? 1 : B1_size / B2_size;
            p = MIN (p - delta, 0);

            replace (&mPageTable[ref_page], p);

            ptr = move_from_B2 (&(mPageTable[ref_page]));
            add_T2_MRU (ptr);
        }

        /**** CASE IV *******/
        else if (mPageTable[ref_page].block_type == NO_BLK) {
            mPageFaultNum++;

            /*==== CASE A ===*/
            if (T1_size + B1_size == mCacheMaxLimit) {
                if (T1_size < mCacheMaxLimit) {
                    del_B1_LRU ();
                    replace (&mPageTable[ref_page], p);
                } else {
                    if (B1_size != 0) {
                        perror ("CASEA Error\n");
                        exit (1);
                    }
                    del_T1_LRU ();
                }
            }

            /*==== CASE B ===*/
            else {
                if (T1_size + B1_size >= mCacheMaxLimit) {
                    perror ("CASEB Error\n");
                    exit (1);
                }
                if (T1_size + T2_size + B1_size + B2_size >= mCacheMaxLimit) {
                    if (T1_size + T2_size + B1_size + B2_size == 2 * mCacheMaxLimit) del_B2_LRU ();
                    replace (&mPageTable[ref_page], p);
                }
            }

            add_T1_MRU (&mPageTable[ref_page]);
        }
    }
    printf ("\n");
    return;
}
}  // namespace replace