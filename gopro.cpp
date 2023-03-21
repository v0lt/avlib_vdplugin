#include "gopro.h"
#include <stdio.h>

// parts from GoProInfo - HYPOXIC

void GoproInfo::get_camera_type(){
  static CameraType types[] = {
    {"HD6.01", HERO6_Black, "GoPro HERO6 Black", "HERO6 Black"},
    {"HD5.03", HERO5_Session, "GoPro HERO5 Session", "HERO5 Session"},
    {"HD5.02", HERO5_Black, "GoPro HERO5 Black", "Hero5-Black Edition"},
    {"HDX.01", HERO4_Session, "GoPro HERO4 Session", "Hero4-Session"},
    {"HD4.02", HERO4_Black, "GoPro HERO4 Black", "Hero4-Black Edition"},
    {"HD4.01", HERO4_Silver, "GoPro HERO4 Silver", "Hero4-Silver Edition"},
    {"HD3.11", HERO3P_Black, "GoPro HERO3+ Black", "Hero3Plus-Black Edition"},
    {"HD3.10", HERO3P_Silver, "GoPro HERO3+ Silver", "Hero3Plus-Silver Edition"},
    {"HD3.03", HERO3_Black, "GoPro HERO3 Black", "Hero3-Black Edition"},
    {"HD3.02", HERO3_Silver, "GoPro HERO3 Silver", "Hero3-Silver Edition"},
    {"HD3.01", HERO3_White, "GoPro HERO3 White", "Hero3-White Edition"},
    {"HD3.20", HERO, "GoPro HERO", ""},
    {"HD3.21", HEROP_LCD, "GoPro HERO+ LCD", ""},
    {"HD3.22", HEROP, "GoPro HERO+", ""}
  };

  int n = sizeof(types)/sizeof(types[0]);

  for(int i=0; i<n; i++){
    if(memcmp(firmware, types[i].Compare,6)==0){
      type = &types[i];
      break;
    }
  }
}

#define arraysize(ar)  (sizeof(ar) / sizeof(ar[0]))
const char *strs_vmode[] = {"Video","Timelapse Video","Looping","Video/Photo"};
const char *strs_fov[] = {"Wide","Medium","Narrow"};

const char *strs_orient[] = {"Up","Down"};

const char *strs_OnOff[] = {"On","Off"};
const char *strs_OffOn[] = {"Off","On"};

const char *strs_pt_wb[] = {"Auto*", "3000K", "5500K", "6500K", "Native"};
const char *strs_pt_color[] = {"Flat","GoPro Color*"};
const char *strs_pt_iso_video[] = {"6400*","3200","1600","800","400"};
const char *strs_pt_sharpness[] = {"Low","Medium","High*"};
const char *strs_pt_ev[] = {"+2","+1.5","+1","+0.5","0","-0.5","-1","-1.5","-2"};
const char str_unknown[] = "Unknown";

const char* DecipherValue(const char** strarray, int max, int value){
  if(value < max)
    return(strarray[value]);
  else
    return str_unknown;
}

void GoproInfo::get_settings(unsigned int* sett, int n)
{
  unsigned int sett1 = sett[0];
  unsigned int sett2 = sett[1];

  // setting 1
  unsigned int mode = sett1 & 0xf;
  unsigned int submode = (sett1 >> 4) & ((1 << 4) - 1);
  unsigned int timelapse_rate = (sett1 >> 8) & ((1 << 4) - 1);
  unsigned int orientation = (sett1 >> 16) & ((1 << 1) - 1);
  unsigned int spotmeter = (sett1 >> 17) & ((1 << 1) - 1);
  unsigned int protune = (sett1 >> 30) & ((1 << 1) - 1);
  //unsigned int white_bal = 0;
  
  // setting 2
  unsigned int fov = (sett2 >> 1) & ((1 << 2) - 1);
  unsigned int lowlight = (sett2 >> 4) & ((1 << 1) - 1);
  unsigned int superview = (sett2 >> 5) & ((1 << 1) - 1);
  unsigned int protune_sharpness = (sett2 >> 6) & ((1 << 2) - 1);
  unsigned int protune_color = (sett2 >> 8) & ((1 << 1) - 1);
  unsigned int protune_iso = (sett2 >> 9) & ((1 << 3) - 1);
  unsigned int protune_ev = (sett2 >> 0xc) & ((1 << 4) - 1);
  unsigned int protune_wb = (sett2 >> 0x10) & ((1 << 2) - 1);
  unsigned int broadcast_privacy = (sett2 >> 0x12) & ((1 << 2) - 1);
  
  char* buf = (char*)malloc(1024);
  buf[0] = 0;
  char* p = buf;
  
  // 0 video
  if(mode){
    sprintf(p,"\tmode:\t%d\r\n",mode); p+=strlen(p);
    sprintf(p,"\tsubmode:\t%d\r\n",submode); p+=strlen(p);
  } else {
    sprintf(p,"\tmode:\t%s\r\n",DecipherValue(strs_vmode,arraysize(strs_vmode),submode)); p+=strlen(p);
  }
  
  sprintf(p,"\torientation:\t%s\r\n",DecipherValue(strs_orient,arraysize(strs_orient),orientation)); p+=strlen(p);
  sprintf(p,"\tspotmeter:\t%s\r\n",DecipherValue(strs_OffOn,arraysize(strs_OffOn),spotmeter)); p+=strlen(p);
  
  if(n>1){
    sprintf(p,"\tfov:\t%s\r\n",DecipherValue(strs_fov,arraysize(strs_fov),fov)); p+=strlen(p);
    
    sprintf(p,"\tlowlight:\t%s\r\n",DecipherValue(strs_OffOn,arraysize(strs_OffOn),lowlight)); p+=strlen(p);
    sprintf(p,"\tsuperview:\t%s\r\n",DecipherValue(strs_OffOn,arraysize(strs_OffOn),superview)); p+=strlen(p);
  }

  sprintf(p,"\tprotune:\t%s\r\n",DecipherValue(strs_OffOn,arraysize(strs_OffOn),protune)); p+=strlen(p);
  if(protune && n>1){
    sprintf(p,"\tprotune_wb:\t%s\r\n",DecipherValue(strs_pt_wb,arraysize(strs_pt_wb),protune_wb)); p+=strlen(p);
    sprintf(p,"\tprotune_color:\t%s\r\n",DecipherValue(strs_pt_color,arraysize(strs_pt_color),protune_color)); p+=strlen(p);
    sprintf(p,"\tprotune_iso:\t%s\r\n",DecipherValue(strs_pt_iso_video,arraysize(strs_pt_iso_video),protune_iso)); p+=strlen(p);
    sprintf(p,"\tprotune_sharpness:\t%s\r\n",DecipherValue(strs_pt_sharpness,arraysize(strs_pt_sharpness),protune_sharpness)); p+=strlen(p);
    sprintf(p,"\tprotune_ev:\t%s\r\n",DecipherValue(strs_pt_ev,arraysize(strs_pt_ev),protune_ev)); p+=strlen(p);
  }

  setup_info = (char*)realloc(buf,strlen(buf)+1);
}

void GoproInfo::find_info(const wchar_t* name)
{
  MovParser parser(name);
  while(1){
    MovAtom a;
    if(!parser.read(a)) return;
    if(a.t=='moov'){
      while(parser.can_read(a)){
        MovAtom b;
        if(!parser.read(b)) return;
        if(b.t=='udta'){
          while(parser.can_read(b)){
            MovAtom c;
            if(!parser.read(c)) return;

            switch(c.t){
            case 'FIRM':
              {
                char* fw;
                int size;
                parser.read(c,fw,size,1);
                firmware = fw;
                get_camera_type();
              }
            break;
                
            case 'LENS':
              break;
                
            case 'CAME':
              {
                char* buf;
                int size;
                parser.read(c,buf,size);
                cam_serial = (char*)malloc(size*2+1);
                {for(int i=0; i<size; i++){
                  int b = ((unsigned char*)buf)[i];
                  sprintf(cam_serial+i*2, "%02x", b); 
                }}
                free(buf);
              }
              break;
                
            case 'SETT':
              {
                unsigned int sett[3] = {0,0,0};
                int n = 1;
                sett[0] = parser.read4();
                if(parser.can_read(c)){
                  sett[1] = parser.read4();
                  n++;
                }
                if(parser.can_read(c)){
                  sett[2] = parser.read4();  // does not exist with GoPRO HERO3
                  n++;
                }
                get_settings(sett,n);
              }
              break;
                
            case 'MUID':
              break;
                
            case 'HMMT':
              break;
            };

            parser.skip(c);
          }
        }
        parser.skip(b);
      }
      break;
    }
    parser.skip(a);
  }
}

