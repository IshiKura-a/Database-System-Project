#pragma once
#include <string>
#include <map>
#include "GlobalData.h"

#define _DEBUG 0

namespace BM
{
    // One block is 0x2000=8192B
    // Default to have NO_BUFFER buffers.
#if _DEBUG
    const uint32_t BLOCK_SIZE = 64;
    const uint32_t NO_BUFFER = 2;
#else
    const uint32_t BLOCK_SIZE = 0x2000;
    const uint32_t NO_BUFFER = 20;
#endif

    // The struct of buffer:
    // beginAddr and endAddr records the range the buffer records
    // accessTimes is recorded to use LRU scheme.
    // isModified records whether the buffer is changed to accelerate disk-write.
    // isPinned is 1 if the buffer cannot be swapped out.
    // isValid is 1 if the buffer has data in it.
    struct buffer {
        uint32_t beginAddr;
        uint32_t endAddr;
        char buf[BLOCK_SIZE];
        uint32_t accessTimes;
        bool isModified;
        bool isPinned;
        bool isValid;
    };

    class BufferManager
    {
    public:
        // ctor and dtor
        BufferManager();
        ~BufferManager();

        // Two ways to read a record at addr of tableName.
        // If missing, read it from disk and add it to buffer.
        // index is the index of tableName in buffer.
        void* Read(std::string& tableName, uint32_t addr, size_t& index);
        void* Read(std::string& tableName, uint32_t addr);

        // Two ways to save buffer to disk.
        // Both of them write from beginAddr to endAddr in the file.
        bool Save(size_t index);
        bool Save(std::string& tableName);

        // Label the buf[index] modified.
        void Set_Modified(size_t index);
        void Set_Modified(std::string& tableName);

        // Label the buf[index] pinned.
        // We cannot swap buf[index] out until we Unset_Pinned it.
        void Set_Pinned(size_t index);
        void Set_Pinned(std::string& tableName);
        void Unset_Pinned(size_t index);
        void Unset_Pinned(std::string& tableName);

        // Free the buffer
        bool Set_Freed(size_t index);
        bool Set_Freed(std::string& tableName);

        // Find the index of tableName.
        // If it doen't exist, return NO_BUFFER and cerr an error.
        size_t Get_Index(std::string& tableName);

        // Append_Record to tableName, return <addr, index>
        std::pair<uint32_t, uint32_t> Append_Record(
            std::string tableName, const Record& row,
            uint32_t addr = UINT32_MAX);
        
        void* Delete_Record(std::string tableName, uint32_t addr);
        bool Drop_Table(std::string& tableName);
        bool Create_Table(std::string& tableName);

        uint32_t Get_Table_Size(std::string& tableName);
    private:
        size_t Get_Free_Buffer();
        void Copy2Buffer(const Record& row, const CM::table& t, char* addr);

        buffer* buf;
        CM::table* tables[NO_BUFFER];
        std::map<std::string,size_t> tableMap;
    };

    inline uint32_t Align(uint32_t x);
    template<class _T> inline _T Min(_T a, _T b);
}