#ifndef __REPLACE_LRU_H__
#define __REPLACE_LRU_H__

#include <set>

#include "replace.h"
#include "trace_handle.h"

namespace replace {

class LRU_Replace : public Replace {
public:
    LRU_Replace (TraceHandle* th, uint32_t mem_size);
    void dyeSample ();
    void Run ();

private:
    typedef struct pf_struct {
        uint32_t ref_times;
        uint32_t pf_times;
        uint32_t page_num;
        bool isResident;
        struct pf_struct* LRU_next;
        struct pf_struct* LRU_prev;
        uint32_t recency;
    } page_struct;

    struct pf_struct* LRU_list_head;
    struct pf_struct* LRU_list_tail;
    std::vector<page_struct> mPageTable;
    std::string filename;

    std::set<uint32_t> dyeBlocks;
};

}  // namespace replace

#endif