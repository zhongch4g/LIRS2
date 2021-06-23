
#include "trace_handle.h"

#include <cstring>
#include <unordered_map>
namespace replace {

TraceHandle::TraceHandle (bool isBinary) {
    mTraceFp = NULL;
    mIsBinary = isBinary;
    mVmSize = 0;
    mTraceLength = 0;
}
TraceHandle::~TraceHandle () {
    if (mTraceFp) {
        fclose (mTraceFp);
    }
    if (mNextPos) {
        delete mNextPos;
    }
}
bool TraceHandle::Init (std::string filename, int32_t cutline) {
    mFileName = filename;
    if (mIsBinary) {
        // printf("Read Binary\n");
        OpenTraceBinary (filename.c_str ());
    } else {
        // printf("Read Text\n");
        OpenTraceText (filename.c_str ());
    }

    fseek (mTraceFp, 0, SEEK_SET);
    uint32_t ref_blk = 0;
    uint32_t max = 0, min = WINT_MAX;
    // key -> pos
    std::unordered_map<uint32_t, uint32_t> prevPos, nextPos;
    prevPos.reserve (mFileSize / 4);
    nextPos.reserve (mFileSize / 4);

    std::unordered_map<uint32_t, uint32_t> dic;
    uint32_t page_num = 0;
    uint32_t max_page_number = 0;
    int32_t cut = 0;
    while (!feof (mTraceFp)) {
        if (cutline != -1) {
            if (cut >= cutline) break;
            cut++;
        }
        ReadOne (&ref_blk);
        // printf("ref_blk: %lu \n", ref_blk);
        if (dic.count (ref_blk) == 0) {
            // not meet before
            mRealTrace[page_num] = ref_blk;
            dic[ref_blk] = page_num++;
        }
        mTrace.push_back (dic[ref_blk]);
        if (dic[ref_blk] > max_page_number) max_page_number = dic[ref_blk];

        auto iter_prePos = prevPos.find (ref_blk);
        if (iter_prePos != prevPos.end ()) {
            // I see previous block,
            // set the next pos of previous block to current pos
            nextPos[prevPos[ref_blk]] = mTraceLength - prevPos[ref_blk];
        }
        prevPos[ref_blk] = mTraceLength;
        if (ref_blk < 0) {
            return false;
        }
        mTraceLength++;
    }

    mVmSize = max_page_number;

    mNextPos = new uint32_t[mTraceLength];
    memset (mNextPos, 0, mTraceLength * sizeof (uint32_t));
    for (auto iter : nextPos) {
        mNextPos[iter.first] = iter.second;
    }
    if (mTrace.size () != mTraceLength) {
        fprintf (stderr, "Trace size not equal.\n");
        return false;
    }
    printf ("TraceHandle len %lu, size: %lu MB \n", mTrace.size (),
            (mTrace.size () * 4) / 1024 / 1024);
    return true;
}

void TraceHandle::ReadOne (uint32_t* ref_blk) {
    if (mIsBinary) {
        size_t flag = fread (ref_blk, 4, 1, mTraceFp);
    } else {
        int flag = fscanf (mTraceFp, "%d", ref_blk);
    }
}

bool TraceHandle::OpenTraceBinary (const char* filename) {
    mTraceFp = fopen (filename, "r");
    if (mTraceFp == NULL) {
        printf ("can not find file %s.\n", filename);
        return false;
    }
    fseek (mTraceFp, 0L, SEEK_END);
    mFileSize = ftell (mTraceFp);
    fseek (mTraceFp, 0L, SEEK_SET);
    return true;
}

void TraceHandle::DecodeToFile () {
    mTraceFp = fopen ((mFileName + "-40k").c_str (), "w");
    uint32_t count = 0;

    for (auto i : mTrace) {
        // fwrite(&i, 4, 1, mTraceFp);
        if (count > 400000) break;
        fprintf (mTraceFp, "%s \n", std::to_string (i).c_str ());
        count++;
    }
    fclose (mTraceFp);
}

bool TraceHandle::OpenTraceText (const char* filename) {
    mTraceFp = fopen (filename, "r");
    if (mTraceFp == NULL) {
        printf ("can not find file %s.\n", filename);
        return false;
    }
    fseek (mTraceFp, 0L, SEEK_END);
    mFileSize = ftell (mTraceFp);
    fseek (mTraceFp, 0L, SEEK_SET);
    return true;
}

}  // namespace replace