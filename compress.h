#ifndef compress_header
#define compress_header

struct CodecClass{
  int class_id;

  CodecClass(){ class_id = 0; }
  virtual ~CodecClass(){}
};

#endif
