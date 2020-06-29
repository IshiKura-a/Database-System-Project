# 总体报告

## Buffer Manager 设计报告

计算机科学与技术学院 计科1804 唐子豪 31801020856  

一、 模块概述  
    Buffer Manager模块主要负责缓冲区的管理。为了提高磁盘的读写效率，我们需要使用缓冲区和文件系统进行交互，批量地读写数据。一般来说，缓冲区与文件系统交互的单位为块，在本实现中为8KB。  

二、 主要功能  
       1. 读取数据到缓冲区。  
       2. 将缓冲区的数据写入文件。
       3. 实现缓冲区的替换算法，在缓冲区满是选择合适的页（Page）进行替换。  
       4. 实现缓冲区的钉住（Pin）功能，锁定该页不允许其被换出。  
       5. 记录缓冲区各页的状态，包括是否有效（isValid）、是否被修改(isModified)、是否钉住(isPinned)等。  

三、 对外接口
    1.  读取数据：
        	返回数据字符串头。根据表名和数据在表中的物理位置读取，index为该表在缓冲区的序号，可以不使用该参数。
        
        ```C++
        void* Read(std::string& tableName, uint32_t addr, size_t& index);
        void* Read(std::string& tableName, uint32_t addr);
        ```
        
   2.  保存数据，写入文件

        ​	保存指定缓冲区的页，写入文件，返回值为一个bool变量，true表示成功，false表示失败。此函数可以通过表名或表在缓冲区的序号进行保存。

        ```C++	
        bool Save(size_t index);
        bool Save(std::string& tableName);
        ```

   3.  标注缓冲区页被修改

        ​	通过表名或缓冲区序号，找到指定页，标注其已被修改（isModified）。

        ```	C++
        void Set_Modified(size_t index);
        void Set_Modified(std::string& tableName);
        ```

   4.  钉住缓冲区页

        ​	接口和上面的一致，被标注为钉住(isPinned)的页面不能被交换。

        ```C++
        void Set_Pinned(size_t index);
        void Set_Pinned(std::string& tableName);
        ```

   5.  解除缓冲区页的钉住状态

        ​	为钉住功能对应的函数，接口一致。

        ```C++
        void Unset_Pinned(size_t index);
        void Unset_Pinned(std::string& tableName);
        ```

   6.  释放缓冲区页

        ​	可以通过表名和缓冲区序号进行释放。注意，在释放时，并不清空buffer的内存，而是重置此页的状态（设置为无效、未修改等），并且把表名从tableMap（记录表名和index的映射关系）中删除。返回值为bool，true表示释放成功，false表示释放失败，可能原因是该缓冲区页被钉住，或不存在此页。

        ```C++
        bool Set_Freed(size_t index);
        bool Set_Freed(std::string& tableName);
        ```

   7.  根据表名获取其在缓冲区的序号

        ​	利用tableMap这一std::map对象，获取表的序号，如果不存在此表，则会返回NO_BUFFER，即缓冲区中页的数量（页的序号从0开始）。

        ```C++
        size_t BufferManager::Get_Index(std::string& tableName);
        ```

   8.  在表中添加数据

        ​	在指定表名的表中插入一条数据，如果地址addr为UINT32_MAX，则附在最后，返回最终插入的位置和缓冲区页的序号。

        ```C++
        std::pair<uint32_t, uint32_t> Append_Record(std::string
        	tableName, const Record& row, uint32_t addr = UINT32_MAX);
        ```

   9.  删除数据

        ​	删除缓冲区的某条数据，返回其在缓冲区的头指针，注意，此删除只将对应的为填0，真正的删除将在Save，即写入磁盘时进行。

        ```C++
        void* Delete_Record(std::string tableName, uint32_t addr);
        ```

   10.  删除表

        ​	删除此表在缓冲区的数据，同时删除对应的文件。返回值为bool值，true表示删除成功。

        ```C++
        bool BufferManager::Drop_Table(std::string& tableName);
        ```

   11.  创建表

         ​	在文件系统中创建以表名为文件名的空文件，在Linux系统下，权限为644，返回值为bool值，true表示创建成功。

         ```C++
         bool BufferManager::Create_Table(std::string& tableName);
         ```

    12.  获取表的大小

         ​	读取表文件，返回数据的个数。

         ```C++
         uint32_t BM::BufferManager::Get_Table_Size(std::string& tableName);
         ```

    13.  内存对齐

         ​	返回一个元组在缓冲区中占据的真实大小，通常是2的整数幂次，依照实现也可能是原大小+1。

         ```C++
         inline uint32_t Align(uint32_t x);
         ```

四、 设计思路

​	为保证缓冲区的效率，缓冲区的块大小设置为8KB，页面个数由常数NO_BUFFER控制。为了能够让缓冲区满足基本的功能，我们需要让buffer记录一些信息，如是否有效、是否被修改、是否被钉住、访问次数、存储数据的首尾地址和存储的数据等。

​	因此，buffer结构定义如下：

```C++
struct buffer {
    uint32_t beginAddr;
    uint32_t endAddr;
    char buf[BLOCK_SIZE];
    uint32_t accessTimes;
    bool isModified;
    bool isPinned;
    bool isValid;
};
```

​	在整个设计当中，需要同时满足表名（用户亲和）和页序号（系统亲和），因此对每个函数都应当有两种重载，为了实现的方便，我们可以记录表名和页序号的映射关系，实现一个函数，然后另一个根据映射关系调用之。如下图所示：

![]()

四、 设计思路

六、 设计思路

 1. 缓冲区的分配与替换

    ​	在本实现中，为每一个表只分配一个缓冲区的页（避免相同数据在不同页中被修改）。表名和页序号的映射，存储在std::map<std::string, int>类型的变量tableMap当中，这样的话，平均查找时间会从$O(N)$降为$O(\log{N})$。缓冲区替换的思路为Least Recent Use(LRU)策略，即使用次数最少的页将被替换出去。

	2.  数据读取冲突解决

    ​	在读取表数据的时候，可能会出现表已经存在，但是数据超出了缓冲区内已经存储的范围的情况，这时候，将会对该页进行一次非换出性替换，即仅写入数据，保留表在缓冲区的信息，然后将需要的数据读入此页。这样做，为的是达到不破坏缓冲区页的钉住和读取数据的折中。

	3.  写入删除数据

    ​	在写入磁盘是，最为简单是是范围写入，通过调用系统函数write，写入一整块数据，但是因为在buffer中，会有一些全0的已删除数据，所以需要进行简单的改进，即以删除的数据为界线，分块写入。

