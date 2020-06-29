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

![重载函数调用关系](https://raw.githubusercontent.com/IshiKura-a/Database-System-Project/master/pic/pic_01.png)

​	在获取了表格数据之后，我们需要将其写入缓冲区中，为了处理方便，我们应当对每一个元组进行内存对齐或者在末尾补上一位标志位。在进行数据删除的时候，直接删除往往会造成高额的时间复杂度，考虑到缓冲区的特性，假删除是一个比较好的选择，可以通过一些标记，标注数据已经被删除，最后在写入磁盘的时候再进行处理。

五、 整体架构

​	整个BufferManager模块分为如下几个部分：缓冲块结构buffer，抽象类BufferManager，对齐函数Align和模板比较函数Min。

​	其中buffer结构已经在上一部分中说明，整个缓冲区管理抽象类的结构如下：

![Buffer Manager整体架构](https://raw.githubusercontent.com/IshiKura-a/Database-System-Project/master/pic/pic_02.png)

​	其中，黑色的均为函数，蓝色的为BufferManager存储的对象，buf即为struct buffer结构的缓存，tables用于存储表信息，来自其他模块，tableMap是表名和页序号的映射。

六、 关键函数与伪代码

 1.  缓冲区的分配与替换

    ​	在本实现中，为每一个表只分配一个缓冲区的页（避免相同数据在不同页中被修改）。表名和页序号的映射，存储在std::map<std::string, int>类型的变量tableMap当中，这样的话，平均查找时间会从$O(N)$降为$O(\log{N})$。

	```C++
tableMap[tableName]=index;
    ```

		缓冲区替换的思路为Least Recent Use(LRU)策略，即使用次数最少的页将被替换出去。
	
	```C++
	size_t Get_Free_Buffer()
	{
	    for(i=0; i<NO_BUFFER; i++)
		{
	    	if there page i is invalid
	    	{
	        	res = i;
	    		break;
	    	}
	    	else
	        	find UNPINNED res in i having the least accessTimes;
		}
	    Save the page if it is necessary.
	    Set_Freed(res);
	    return res;
	}
	
	```

2.  数据读取冲突解决

   ​	在读取表数据的时候，可能会出现表已经存在，但是数据超出了缓冲区内已经存储的范围的情况，这时候，将会对该页进行一次非换出性替换，即仅写入数据，保留表在缓冲区的信息，然后将需要的数据读入此页。这样做，为的是达到不破坏缓冲区页的钉住和读取数据的折中。

   ```C++
   void* Read(std::string tableName, uint_32 addr, ...)
   {
       index = Get_Index(tableName);
       if(index != NO_BUFFER)
       {
           ...
           if addr not in buf[index]
           {
               Save(index);
               Reread it from disk and fresh buf[index];
               return buf[index].buf;
           }
       }
       ...
   }
   ```

3.  写入删除数据

   ​	在写入磁盘是，最为简单是是范围写入，通过调用系统函数write，写入一整块数据，但是因为在buffer中，会有一些全0的已删除数据，所以需要进行简单的改进，即以删除的数据为界线，分块写入。

   ```C++
   bool Save(...)
   {
       open the file;
       for all blocks between zero-records
       {
           write thr block;
       }
       return true;
   }
   ```

 4.  添加数据

    ​	如果地址在范围内，直接覆盖，如果addr是UINT32_MAX，插入到buffer末尾，若此时缓冲区满，则保存。

    ```C++
    std::pair<uint32_t, uint32_t> Append_Record(std::string tableName, const Record& row, uint32_t addr)
    {
        if addr != UINT32_MAX
            find the record and cover it;
        else
        {
            find the table in buffer;
            if buf[i].isValid && buf[i].beginAddr == the number of tuples in the file
            {
                append it;
                update buf[i].endAddr;
                Save(i) if buf is full and
                	return <buf[i].endAddr - 1. NO_BUFFER>;
                else return <buf[i].endAddr - 2, i>
            }
            else
            {
                Reread it;
                Append the record;
                return;
            }
        }
    }
    ```

七、 基础测试

​	为了验证此模块的基本正确性，我们调整了模块中的参数，使得每个缓冲区页只能存储4条数据，只有2页。这样我们能够更快地达到我们需要的测试情况，测试分段进行。因为模块中会调用其他模块的函数，所以使用了GlobalData.h进行了模拟。由于编译环境的问题，需要屏蔽4996报错信号，保证系统函数read()和write()正确运行。

```	C++
#include "BufferManager.h"
#include "GlobalData.h"

#include <iostream>

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

int main()
{
	BM::BufferManager *a = new BM::BufferManager;
```

​	一开始检验程序基本正确性，尝试建立、删除表tmp。注释部分为系统函数的测试。

```	C++
	std::string s1 = "tmp";
/*	uint32_t fd = open(s1.c_str(), O_CREAT, 0644);
	close(fd);

	char buf[4] = "413";
	fd = open(s1.c_str(), O_WRONLY);
	write(fd, buf, 4);
	close(fd);

	unlink(s1.c_str());*/
	
	Record r;
	a->Create_Table(s1);
	a->Drop_Table(s1);
```

​	结果在目录中出现了tmp文件，随即被删除。

![基本测试创建、删除](https://raw.githubusercontent.com/IshiKura-a/Database-System-Project/master/pic/pic_03.png)

​	接下来测试Append_Record和Save，插入4条数据。

```C++
	s1 = "Y";
//  a->Create_Table(s1);
	std::string f1[10] = { "65","66","67","68" };

	int i;
	for (i = 0; i < 4; i++)
	{
		r.clear();
		r.push_back(f1[i]);
		r.push_back("1.6");
		r.push_back("#");
		a->Append_Record(s1, r, UINT32_MAX);
	}
```

​	文件Y中出现了如下4条数据，说明基本功能正确。

![插入数据测试](https://raw.githubusercontent.com/IshiKura-a/Database-System-Project/master/pic/pic_04.png)

​	接下来，覆盖数据0，插入新数据，删除数据3，4。

```C++
	r.clear();
	r.push_back("69");
	r.push_back("1.6");
	r.push_back("#");
	a->Append_Record(s1, r, 0);

	r.clear();
	r.push_back("70");
	r.push_back("1.6");
	r.push_back("#");
	a->Append_Record(s1, r, UINT32_MAX);
	
	a->Delete_Record(s1, 3);
	a->Delete_Record(s1, 4);
```

​	此时文件中仍然包含5个数据，说明删除操作仍在缓冲区中。

![覆盖、删除数据测试](https://raw.githubusercontent.com/IshiKura-a/Database-System-Project/master/pic/pic_05.png)

​	接下来进行替换测试：

```C++
	std::string s2 = "Y2";
	// a->Create_Table(s2);
	r.clear();
	r.push_back("71");
	r.push_back("1.6");
	r.push_back("#");
	a->Append_Record(s2, r, UINT32_MAX);
	// a->Set_Pinned(s2);

	std::string s3 = "Y3";
	// a->Create_Table(s3);
	r.clear();
	r.push_back("72");
	r.push_back("1.6");
	r.push_back("#");
	a->Append_Record(s3, r, UINT32_MAX);



	return 0;

}
```

​	注释钉住Y的那行代码，结果Y文件内容不变（仍在缓冲区中），Y2文件写出。

![替换测试-1](https://raw.githubusercontent.com/IshiKura-a/Database-System-Project/master/pic/pic_06.png)

​	取消注释，Y2文件大小为0，没有被写入，Y文件被更新，结果正确。

![替换测试-2](https://raw.githubusercontent.com/IshiKura-a/Database-System-Project/master/pic/pic_07.png)

