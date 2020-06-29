#include "BufferManager.h"
#include <iostream>
#include <cstring>

// POSIX api
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _UNIX
#include <unistd.h>
#endif

#if defined (_WIN32) || defined (_WIN64)
#include <io.h>
#endif

#pragma warning(disable: 4996)

uint32_t BM::Align(uint32_t x)
{
    // An algorithm to round x up to the power of 2
    // 2->2, 3->4, 33->64
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}
template<class _T> inline _T BM::Min(_T a, _T b)
{
    return (a < b) ? a : b;
}
BM::BufferManager::BufferManager()
{
    buf = new buffer[NO_BUFFER];
    size_t i;
    for(i=0;i<NO_BUFFER;i++)
    {
        buf[i].accessTimes = 0;
        buf[i].isModified = buf[i].isPinned
            = buf[i].isValid = false;
        tables[i] = nullptr;
        memset(buf[i].buf, 0, BLOCK_SIZE*sizeof(char));
    }
}
BM::BufferManager::~BufferManager()
{
    size_t i;
    for(i=0;i<NO_BUFFER;i++)
    {
        Save(i);
    }
    delete[] buf;
}

void BM::BufferManager::Set_Modified(size_t index)
{
    buf[index].isModified = true;
}
void BM::BufferManager::Set_Modified(std::string& tableName)
{
    size_t index = Get_Index(tableName);
    if(index < NO_BUFFER)
        Set_Modified(index);
}

void BM::BufferManager::Set_Pinned(size_t index)
{
    buf[index].isPinned = true;
}
void BM::BufferManager::Set_Pinned(std::string& tableName)
{
    size_t index = Get_Index(tableName);
    if(index < NO_BUFFER)
        Set_Pinned(index);
}
void BM::BufferManager::Unset_Pinned(size_t index)
{
    buf[index].isPinned = false;
}
void BM::BufferManager::Unset_Pinned(std::string& tableName)
{
    size_t index = Get_Index(tableName);
    if(index < NO_BUFFER)
        Unset_Pinned(index);
}

bool BM::BufferManager::Set_Freed(size_t index)
{
    if(buf[index].isPinned)
    {
        std::cerr << "Set_Freed: Free pinned buffer " <<
           index << " !\n";
        return false;
    }else
    {
        if(tables[index])
            tableMap.erase(tables[index]->name);
        memset(buf[index].buf, 0, BLOCK_SIZE * sizeof(char));
        buf[index].isValid =
        buf[index].isModified =
        buf[index].isPinned = false;
        tables[index] = nullptr;
        return true;
    }
}
bool BM::BufferManager::Set_Freed(std::string& tableName)
{
    size_t index = Get_Index(tableName);
    if(index < NO_BUFFER)
        return Set_Freed(index);
    else return false;
}

bool BM::BufferManager::Create_Table(std::string& tableName)
{

#ifdef _UNIX
    // 0644 means rw-r--r--
    int32_t fd = open(tableName.c_str(), O_CREAT, 0644);
#endif

#if defined (_WIN32) || defined (_WIN64)
    int32_t fd = open(tableName.c_str(), O_CREAT, 0644);
#endif
    
    close(fd);
    return (fd != -1);
}
bool BM::BufferManager::Drop_Table(std::string& tableName)
{
    // If the table is in the buffer, we should free the buffer,
    size_t i = Get_Index(tableName);
    if(i < NO_BUFFER)
    {
        // The buffer should not be pinned.
        Unset_Pinned(i);
        Set_Freed(i);
    }
    // delete the file tableName
    int32_t res = 0;
    res = unlink(tableName.c_str());
    return (res == 0);
}

void* BM::BufferManager::Read(std::string& tableName, uint32_t addr, size_t& index)
{
    // Read from buffer first
    size_t i = Get_Index(tableName);

    if (i < NO_BUFFER)
    {
        if (buf[i].beginAddr <= addr && buf[i].endAddr >= addr)
        {
            index = i;
            buf[i].accessTimes++;
            return buf[i].buf + (addr - buf[i].beginAddr) *
                BM::Align(tables[i]->sizePerTuple);
        }
        Save(i);
    }

#if _DEBUG
    CM::table& t = test::Get_table(tableName);
#else
    CM::table& t = MiniSQL::get_catalog_manager().get_table(tableName);
#endif

    int32_t fd = open(tableName.c_str(),O_RDONLY);
    if(fd == -1) return nullptr;

    uint32_t fileSize = lseek(fd, 0, SEEK_END);
    if(addr > fileSize / Align(t.sizePerTuple))
    {
        throw std::out_of_range("BM: addr is out of range!");
    }

    if(i == NO_BUFFER)
        i = Get_Free_Buffer();
    tables[i] = &t;
    tableMap[tableName] = i;
    buf[i].isValid = true;
    buf[i].accessTimes = 1;
    buf[i].isModified = false;
    Unset_Pinned(i);
    tableMap[tableName] = i;
    buf[i].beginAddr = addr;

    uint32_t alignSize = Align(t.sizePerTuple);
    buf[i].endAddr = addr + Min(BLOCK_SIZE / alignSize,
        fileSize / alignSize - addr);
    
    lseek(fd, addr * alignSize, SEEK_SET);
    read(fd, buf[i].buf, (buf[i].endAddr - addr) * alignSize);
    close(fd);
    index = i;
    return buf[i].buf;
}
void* BM::BufferManager::Read(std::string& tableName, uint32_t addr)
{
    size_t index;
    return Read(tableName, addr, index);
}

bool BM::BufferManager::Save(size_t index)
{
    std::string tableName = tables[index]->name;
    // if(buf[index].isPinned) return false;

    if(buf[index].isModified)
    {
#ifdef _UNIX
    int32_t fd = open(tableName.c_str(), O_WRONLY, 0644);
#endif

#if defined (_WIN32) || defined (_WIN64)
    int32_t fd = open(tableName.c_str(), O_WRONLY);
#endif

        if(fd == -1) return false;

        /*
        size_t inc = Align(tables[index]->sizePerTuple);
        size_t begin, end;
        int32_t bias;
        char* _ = new char[inc];
        memset(_, 0, inc * sizeof(char));

        // We need to delete zero records which don't exist.
        bias = buf[index].beginAddr * inc;
        begin = 0; end = 0;
        while(end / inc + 1 <= buf[index].endAddr - buf[index].beginAddr)
        {
            if(strncmp(buf[index].buf + end, _, inc))
            {
                end += inc;
            }else
            {
                // Write non-zero blocks.
                if (end - begin != 0)
                {
                    lseek(fd, begin + bias, SEEK_SET);
                    write(fd, buf[index].buf + begin, (end - begin));
                }
                begin = end = end + inc;
                bias -= inc;
            }
            
        }
        lseek(fd, begin + bias, SEEK_SET);
        write(fd, buf[index].buf + begin, (end - begin));
        close(fd);
        */

        uint32_t alignSize = Align(tables[index]->sizePerTuple);
        lseek(fd, buf[index].beginAddr * alignSize, SEEK_SET);
        write(fd, buf[index].buf, (buf[index].endAddr - buf[index].beginAddr) * alignSize);
        close(fd);
    }
    buf[index].accessTimes = 0;
    buf[index].isModified = buf[index].isValid = false;

    return true;
    
}
bool BM::BufferManager::Save(std::string& tableName)
{
   size_t index = Get_Index(tableName);
   if(index < NO_BUFFER)
   {
       return Save(index);
   }else return false;
}

std::pair<uint32_t, uint32_t> BM::BufferManager::Append_Record(
    std::string tableName, const Record& row, uint32_t addr)
{
    if(addr != UINT32_MAX)
    {
        size_t i = Get_Index(tableName);
        if(i < NO_BUFFER)
        {
            // If the record is right in the buf, cover it.
            // Meanwhile, increace the accessTimes of it and
            // label the buffer modified.
            if(buf[i].isValid && buf[i].beginAddr <= addr
                && buf[i].endAddr >= addr)
            {
                buf[i].accessTimes++;
                Set_Modified(i);
                Copy2Buffer(row, *(tables[i]), buf[i].buf + Align(tables[i]->sizePerTuple)*
                    (addr - buf[i].beginAddr));
                return std::make_pair(addr, i);
            }
            
            // In order to avoid write-conflict, we can only build one
            // buffer for a table. Hence the buffer must be write to disk.
            // However we need not swap it out for it may be pinned.
            // We will add a new range of records to the buffer afterwards.
            Save(i);
        }

        // record id not inclued in the buffer, read the table.
#if _DEBUG
        CM::table& t = test::Get_table(tableName);
#else
        CM::table& t = MiniSQL::get_catalog_manager().get_table(tableName);
#endif
        int32_t fd = open(t.name.c_str(), O_RDONLY, S_IREAD);

        // If the table doesn't exist create one.
        if(fd == -1)
        {
            Create_Table(tableName);
            fd = open(t.name.c_str(), O_RDONLY, S_IREAD);
        }

        // Get the size of the file.
        uint32_t fileSize = lseek(fd, 0, SEEK_END);
        if(addr >= fileSize / Align(t.sizePerTuple))
        {
            throw std::out_of_range("BM Append_Record: out of range!");
        }

        // If we didn't find the table, we ask for a new buffer.
        // Otherwise we use the buffer above.
        if(i == NO_BUFFER)
            i = Get_Free_Buffer();

        // Initialize
        tables[i] = &t;
        tableMap[tableName] = i;
        Set_Modified(i);
        buf[i].isValid = true;
        buf[i].accessTimes = 1;
        buf[i].beginAddr = addr;

        // Mind that all the tuples are aligned to power of 2.
        uint32_t alignSize = Align(t.sizePerTuple);
        buf[i].endAddr = addr + Min(BLOCK_SIZE / alignSize,
        fileSize / alignSize - addr);

        // Get to the addrth record and read them from buffer.
        lseek(fd, addr * alignSize, SEEK_SET);
        read(fd, buf[i].buf, (buf[i].endAddr - addr) * alignSize);
        close(fd);

        Copy2Buffer(row, *tables[i], buf[i].buf);
        return std::make_pair(addr, i);
    }
    else
    {
        // addr = UINT32_MAX, it means we should append a new record
        // to the table, but we don't know the addr.
#if _DEBUG
        CM::table& t = test::Get_table(tableName);
#else
        CM::table& t = MiniSQL::get_catalog_manager().get_table(tableName);
#endif
        int32_t fd = open(t.name.c_str(), O_RDONLY, S_IREAD);

        if (fd == -1)
        {
            Create_Table(tableName);
            fd = open(t.name.c_str(), O_RDONLY, S_IREAD);
        }

        uint32_t fileSize = lseek(fd, 0, SEEK_END);
        uint32_t _endAddr = fileSize / Align(t.sizePerTuple);

        size_t i = Get_Index(tableName);
        if(i < NO_BUFFER)
        {
            // If the table is in the buffer and it's appending
            // record, we append the row to the buffer.
            if(buf[i].isValid && buf[i].beginAddr == _endAddr)
            {
                buf[i].accessTimes++;
                Set_Modified(i);
                // endAddr should ++.
                Copy2Buffer(row, t, buf[i].buf + Align(t.sizePerTuple)*
                    (buf[i].endAddr++ - buf[i].beginAddr));
                
                // The buffer is full, write to disk.
                if(buf[i].endAddr - buf[i].beginAddr >= BLOCK_SIZE / Align(t.sizePerTuple))
                {
                    Save(i);
                    Set_Freed(i);
                    // return NO_BUFFER means the buffer no more exists.
                    return std::make_pair(buf[i].endAddr - 1, NO_BUFFER);
                }

                return std::make_pair(buf[i].endAddr - 1, i);
            }

            // Still we can only assign one buffer to a table.
            Save(i);
            memset(buf[i].buf, 0, Align(t.sizePerTuple)*
                (buf[i].endAddr - buf[i].beginAddr));
        }

        if(i == NO_BUFFER)
            i = Get_Free_Buffer();
        buf[i].accessTimes = 1;
        Set_Modified(i);
        buf[i].isValid = true;
        tables[i] = &t;
        tableMap[tableName] = i;
        // Only one record is in the buffer.
        buf[i].beginAddr = _endAddr;
        buf[i].endAddr = _endAddr + 1;
        Copy2Buffer(row, t, buf[i].buf);
        return std::make_pair(_endAddr, i);
    }
    
    
}

void BM::BufferManager::Copy2Buffer(const Record& row, const CM::table& t, char* addr)
{
    size_t i;
    uint32_t tmpInt;
    float tmpFloat;
    char* oldAddr = addr;
    // Write the tuple to buffer.
    for(i = 0; i < t.NOF; i++)
    {
        switch (t.fields[i].type)
        {
        case INT:
            tmpInt = std::stoi(row[i]);
            memcpy(addr, (const char*)(&tmpInt), t.fields[i].N);
            addr += t.fields[i].N;
            break;
        case CHAR_N:
            memcpy(addr, row[i].c_str(), row[i].size());
            addr += row[i].size();
            // memery alignment in string
            memset(addr, 0, t.fields[i].N - row[i].size());
            addr += t.fields[i].N - row[i].size();
            break;
        case FLOAT:
            tmpFloat = std::stof(row[i]);
            memcpy(addr, (const char*)(&tmpFloat), t.fields[i].N);
            addr += t.fields[i].N;
            break;
        default:
            break;
        }
    }
    // Align each tuple to power of two.
    memset(addr, 0, Align(t.sizePerTuple) - t.sizePerTuple);
    // recover addr.
    addr = oldAddr;
}

void* BM::BufferManager::Delete_Record(std::string tableName, uint32_t addr)
{
    size_t i;
    void* p = Read(tableName, addr, i);
    // Set the filed of this record to zeros.
    memset(reinterpret_cast<char*>(p), 0, Align(tables[i]->sizePerTuple));
    Set_Modified(i);

    return p;
}

uint32_t BM::BufferManager::Get_Table_Size(std::string& tableName)
{
    // Get the info of the table.
#if _DEBUG
    CM::table& t = test::Get_table(tableName);
#else
    // CM::table& t = MiniSQL::get_catalog_manager().get_table(tableName);
#endif
    int32_t fd = open(tableName.c_str(), O_RDONLY, S_IREAD);

    if (fd == -1)
    {
        std::cerr << tableName << "doesn't exist!\n";
        return UINT32_MAX;
    }
    uint32_t fileSize = lseek(fd, 0, SEEK_END);

    return fileSize / Align(t.sizePerTuple);
}

size_t BM::BufferManager::Get_Free_Buffer()
{
    // LRU swap-out scheme
    size_t i, index = 0;
    uint32_t minAccessTime = 0xFFFFFFFF;

    for(i = 0; i < NO_BUFFER; i++)
    {
        // If some buffer is empty, use it.
        if(!buf[i].isValid)
        {
            index = i;
            break;
        }else
        {
            // The buffer should not be pinned.
            if(buf[i].accessTimes < minAccessTime && !buf[i].isPinned)
            {
                index = i;
                minAccessTime = buf[i].accessTimes;
            }
        }
    }

    // If the buffer is not modified, we do not write them to disk.
    if(buf[index].isValid && buf[index].isModified)
        Save(index);
    Set_Freed(index);
    return index;
}

size_t BM::BufferManager::Get_Index(std::string& tableName)
{
    // In case table doesn't exist, we use try-catch.
    try
    {
        // at can return std::out_of_range
        size_t index = tableMap.at(tableName);
        return index;
    }
    catch(const std::exception& e)
    {
        // return NO_BUFFER to show the table doesn't exist.
        // std::cerr << e.what() << '\n';
        return NO_BUFFER;
    }
}