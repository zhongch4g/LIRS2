#ifndef __REPLACE_ARC_H__
#define __REPLACE_ARC_H__

#include "replace.h"
#include "trace_handle.h"

namespace replace {
#define T2_ 0.4
#define Td 10
#define Tout (T2_ + Td)

#define TRUE 1
#define FALSE 0

#define NO_BLK 0
#define T1_BLK 1
#define B1_BLK 2
#define T2_BLK 3
#define B2_BLK 4

class ARC_Replace : public Replace {
public:
    ARC_Replace (TraceHandle* th, uint32_t mem_size);
    void Run ();

private:
    typedef struct pf_struct {
        uint32_t ref_times;
        uint32_t pf_times;
        uint32_t page_num;
        bool isResident;
        int block_type;
        struct pf_struct* next;
        struct pf_struct* prev;
        uint32_t recency;
    } page_struct;

    /* through the replacement to force the size of T1 around p */
    int replace (page_struct* pg_ptr, int p);
    int add_B1_MRU (struct pf_struct* ptr);

    int add_B2_MRU (struct pf_struct* ptr);
    uint32_t MIN (uint32_t a, uint32_t b);
    struct pf_struct* move_from_T1 (struct pf_struct* ptr);
    struct pf_struct* move_from_T2 (struct pf_struct* ptr);
    struct pf_struct* del_T1_LRU ();
    struct pf_struct* del_T2_LRU ();
    struct pf_struct* move_from_B1 (struct pf_struct* ptr);
    struct pf_struct* move_from_B2 (struct pf_struct* ptr);
    int add_T1_MRU (struct pf_struct* ptr);
    int add_T2_MRU (struct pf_struct* ptr);
    struct pf_struct* del_B1_LRU ();
    struct pf_struct* del_B2_LRU ();

    struct pf_struct *T1_list_head, *T1_list_tail;
    struct pf_struct *B1_list_head, *B1_list_tail;
    struct pf_struct *T2_list_head, *T2_list_tail;
    struct pf_struct *B2_list_head, *B2_list_tail;

    uint32_t T1_size, T2_size, B1_size, B2_size;

    uint32_t STAT_START_POINT;
    std::vector<page_struct> mPageTable;
};
}  // namespace replace
#endif