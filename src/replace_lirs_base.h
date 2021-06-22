#ifndef __REPLACE_LIRS_BASE_H__
#define __REPLACE_LIRS_BASE_H__
#include <cassert>
#include <map>
#include <vector>

#include "replace.h"
namespace replace {

#define S_STACK_IN true
#define S_STACK_OUT false

typedef struct base {
    uint32_t page_num;
    bool isResident;
    int isHIR_block;
    int32_t refer_time;
    bool isDemote;
    uint32_t recency2;
    int score;
    uint32_t r_or_k;
    uint32_t prediction;
    std::map<uint32_t, bool> virtual_time_to_LR_status;

} base_info;

typedef struct pf_struct {
    // lirs_base
    uint32_t ref_times;
    uint32_t pf_times;
    uint32_t page_num;
    bool isResident;
    int isHIR_block;
    struct pf_struct* LIRS_next;
    struct pf_struct* LIRS_prev;
    struct pf_struct* HIR_rsd_next;
    struct pf_struct* HIR_rsd_prev;
    uint32_t recency;

    // lirs2
    bool fake_ins;
    uint32_t which_instance;
    std::vector<uint32_t> reuse_distance;

    // Three status, STACK_IN STACK_OUT NULL
    uint32_t recency0;
    uint32_t recency1;
    uint32_t recency2;
    uint32_t refer_time;
} page_struct;

class LIRS_Replace : public Replace {
public:
    LIRS_Replace (){};
    LIRS_Replace (TraceHandle* th, uint32_t mem_size);
    void Report () override;
    virtual void Run ();

public:
    /* record the evicted page from HIR queue */
    virtual void recordEvict (uint32_t page_num);
    virtual bool removeHIRList (page_struct* HIR_block_ptr);
    /* remove a block from memory */
    virtual bool removeLIRSList (page_struct* page_ptr);
    virtual page_struct* findLastLirLru ();
    /* put a newly referenced block on the top of LIRS stack */
    virtual void addLruListHead (page_struct* new_ref_ptr);
    /* put a HIR resident block on the end of HIR resident list */
    virtual void addHirlistHead (page_struct* new_rsd_HIR_ptr);
    /* To address an extreme case, in which the size of LIR stack
     * is larger than the limitation(MAX_S_LEN).
     */
    virtual page_struct* pruneLIRSstack ();
    /* insert a block in LIRS list */
    virtual void insertLRUList (page_struct* old_ref_ptr, page_struct* new_ref_ptr);

    struct pf_struct* LRU_list_head;
    struct pf_struct* LRU_list_tail;

    struct pf_struct* HIR_list_head;
    struct pf_struct* HIR_list_tail;

    struct pf_struct* LIR_LRU_block_ptr; /* LIR block with Rmax recency */

    uint32_t HIR_block_portion_limit, HIR_block_activate_limit;

    uint32_t evict_cur_idx, evict_max_idx;
    std::vector<uint32_t> evict_list;

    uint32_t PRE_REFER_FLAG;

    // current lir stack length
    uint32_t cur_lir_S_len;

    uint32_t EVICT_LIST_SIZE;
    // give Queue Q a minimum size
    uint32_t LOWEST_HG_NUM;
    /* this specifies from what virtual time (reference event), the counter for
     * block miss starts to collect. You can test a warm cache by changin the "0"
     * to some virtual time you desire.
     * */
    uint32_t STAT_START_POINT;
    /* the size percentage of HIR blocks, default value is 1% of cache size */
    uint32_t HIR_RATE;
    /* if you want to limit the maximum LIRS stack size (e.g. 3 times of LRU stack  *  size,
     * you can change the "2000" to "3"*/
    uint32_t MAX_S_LEN;
    std::vector<page_struct> mPageTable;
};

}  // namespace replace
#endif