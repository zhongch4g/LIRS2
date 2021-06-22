#ifndef __REPLACE_OPT_H__
#define __REPLACE_OPT_H__

#include <set>

#include "replace.h"
#include "trace_handle.h"

namespace replace {

class OPT_Replace : public Replace {
public:
    OPT_Replace (TraceHandle* th, uint32_t mem_size);
    void showStack ();
    void Run ();

private:
    typedef struct pf_struct {
        uint32_t fwd_distance;
        bool isResident;
        uint32_t ref_times;
    } page_struct;

    struct MyCompare {
        bool operator() (const page_struct* a, const page_struct* b) {
            return a->fwd_distance < b->fwd_distance;
        }
    };
    std::vector<page_struct> mPageTable;
    std::set<page_struct*, MyCompare> mCache;
};

}  // namespace replace

#endif