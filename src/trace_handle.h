#ifndef __TRACE_HANDLE_H__
#define __TRACE_HANDLE_H__

#include <string>
#include <vector>
#include "sparsepp/spp.h"
namespace replace
{

class TraceHandle
{
public:
    TraceHandle(bool isBinary);
    ~TraceHandle();
    bool Init(std::string filename, int32_t cutline);

    void ReadOne(uint32_t *ref_blk);

    bool OpenTraceBinary(const char *filename);

    bool OpenTraceText(const char *filename);

    void DecodeToFile();

    std::string mFileName;
    FILE *mTraceFp;
    size_t mFileSize;
    bool mIsBinary;
    uint32_t mVmSize;
    uint32_t mTraceLength;
    uint32_t *mNextPos;
    std::vector<uint32_t> mTrace;
    spp::sparse_hash_map<uint32_t, uint32_t> mRealTrace;
};
    
} // namespace name


#endif