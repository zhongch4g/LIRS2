#ifndef __REPLACE_LIRS2_H__
#define __REPLACE_LIRS2_H__

#include "replace_lirs_base.h"

namespace replace {

class LIRS2_Replace : public LIRS_Replace {
public:
    LIRS2_Replace (TraceHandle* th, uint32_t mem_size);
    void Report () override;
    void Run ();

private:
    /* record the evicted page from HIR queue */
    void recordEvict (uint32_t page_num);
    bool removeHIRList (page_struct* HIR_block_ptr);
    /* remove a block from memory */
    bool removeLIRSList (page_struct* page_ptr);
    bool removeLIRSListWithoutPruning (page_struct* page_ptr);
    page_struct* findLastLirLru (uint32_t which_instance);
    /* put a newly referenced block on the top of LIRS stack */
    void addLruListHead (page_struct* new_ref_ptr);
    /* put a HIR resident block on the end of HIR resident list */
    void addHirlistHead (page_struct* new_rsd_HIR_ptr);
    /* To address an extreme case, in which the size of LIR stack
     *  is larger than the limitation(MAX_S_LEN).*/
    page_struct* pruneLIRSstack ();
    /* insert a block in LIRS list */
    void insertLRUList (page_struct* old_ref_ptr, page_struct* new_ref_ptr);

    page_struct* findInstanceInQ (uint32_t block_number);

    page_struct* pointerInstance (uint32_t block_number, uint32_t which_instance);
    page_struct* findPageTable (uint32_t block_number);

    struct pf_struct* LRU_list_head;
    struct pf_struct* LRU_list_tail;

    struct pf_struct* HIR_list_head;
    struct pf_struct* HIR_list_tail;

    struct pf_struct* Rmax1; /* LIR block with Rmax1 recency */
    struct pf_struct* Rmax2; /* LIR block with Rmax2 recency */

    uint32_t HIR_block_portion_limit, HIR_block_activate_limit;

    /* current lir stack length */
    uint32_t cur_ins1_rmax1_len;
    uint32_t cur_ins2_rmax2_len;
    uint32_t cur_lir_S_len;

    uint32_t EVICT_LIST_SIZE;
    /* give Queue Q a minimum size */
    uint32_t LOWEST_HG_NUM;

    /* the size percentage of HIR blocks, default value is 1% of cache size */
    uint32_t HIR_RATE;
    /* if you want to limit the maximum LIRS stack size (e.g. 3 times of LRU stack  *  size,
     * you can change the "2000" to "3"*/
    uint32_t MAX_S_LEN;

    std::string filename;

    std::vector<base_info> mPageTable;
    std::vector<page_struct> mPageTable1;
    std::vector<page_struct> mPageTable2;
};

}  // namespace replace

#endif