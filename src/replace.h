#ifndef __REPALCE_H__
#define __REPALCE_H__

#include <sys/stat.h>
#include <sys/types.h>

#include <string>

namespace replace {

class TraceHandle;

class Replace {
public:
    TraceHandle *mTraceHandle;
    uint32_t mCacheMaxLimit;
    uint32_t mPageFaultNum;
    uint32_t mFreeMemSize;
    uint32_t valid_refer_page;
    uint32_t ColdMiss;
    std::string mName;

    virtual void Report ();
};

}  // namespace replace

#endif