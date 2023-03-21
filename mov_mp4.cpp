#include "mov_mp4.h"

// copied from quicktime driver
#define bswap32(x) \
{ unsigned long eax = *((unsigned long *)&(x)); \
  *((unsigned long *)&(x)) = (eax >> 24) | ((eax >> 8) & 0x0000FF00UL) | ((eax << 8) & 0x00FF0000UL) | (eax << 24); \
}

#define bswap64(x) \
{ unsigned long eax = *((unsigned long *)&(x) + 0); \
  unsigned long edx = *((unsigned long *)&(x) + 1); \
  *((unsigned long *)&(x) + 0) = (edx >> 24) | ((edx >> 8) & 0x0000FF00UL) | ((edx << 8) & 0x00FF0000UL) | (edx << 24); \
  *((unsigned long *)&(x) + 1) = (eax >> 24) | ((eax >> 8) & 0x0000FF00UL) | ((eax << 8) & 0x00FF0000UL) | (eax << 24); \
}

unsigned long MovParser::read4(){
  unsigned long r;
  if(hfile){
    unsigned long w;
    ReadFile(hfile,&r,4,&w,0);
  } else {
    r = *(unsigned long*)p; p+=4;
  }
  offset+=4;
  bswap32(r);
  return r;
}

__int64 MovParser::read8(){
  __int64 r;
  if(hfile){
    unsigned long w;
    ReadFile(hfile,&r,8,&w,0);
  } else {
    r = *(__int64*)p; p+=8;
  }
  offset+=8;
  bswap64(r);
  return r;
}

bool MovParser::read(MovAtom& a)
{
  if(!can_read(8)) return false;
  a.pos = offset;
  a.sz = read4();
  a.t = read4();
  a.hsize = 8;

  if(a.sz==0){
    a.sz = fileSize;
  } else if(a.sz==1){
    if(!can_read(8)) return false;
    a.sz = read8();
    if(a.sz<16) return false;
    a.hsize = 16;
  } else if(a.sz<8){
    return false;
  }

  return true;
}

