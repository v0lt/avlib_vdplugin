#ifndef fopen_header
#define fopen_header

#include <stdio.h>

__forceinline FILE* fopen_test(const char* name, const char* mode){
	if(!name) return 0;
	if(name[0]=='?') return 0;
	return fopen(name,mode);
}

#define fopen(name,mode) fopen_test(name,mode)

#endif
