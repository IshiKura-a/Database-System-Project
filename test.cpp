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

	s1 = "Y";
	// a->Create_Table(s1);
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