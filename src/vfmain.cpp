#include <vd2/VDXFrame/VideoFilterEntry.h>
#include "fflayer.h"

VDX_DECLARE_VIDEOFILTERS_BEGIN()
  VDX_DECLARE_VIDEOFILTER(filterDef_fflayer)
VDX_DECLARE_VIDEOFILTERS_END()

extern "C" int __cdecl VirtualdubFilterModuleInit2(VDXFilterModule *fm, const VDXFilterFunctions *ff, int& vdfd_ver, int& vdfd_compat) {
  if(VDXVideoFilterModuleInit2(fm,ff,vdfd_ver)) return 1;

  vdfd_ver = VIRTUALDUB_FILTERDEF_VERSION;
  vdfd_compat = 9;		// copy constructor support required
  return 0;
}

extern "C" int __cdecl FilterModModuleInit(VDXFilterModule *fm, const FilterModInitFunctions *ff, int& vdfd_ver, int& vdfd_compat, int& mod_ver, int& mod_min) {
  if(VDXVideoFilterModuleInitFilterMod(fm,ff,vdfd_ver,mod_ver)) return 1;

  vdfd_ver = VIRTUALDUB_FILTERDEF_VERSION;
  vdfd_compat = 9;		// copy constructor support required

  mod_ver = FILTERMOD_VERSION;
  mod_min = FILTERMOD_VERSION;
  return 0;
}

extern "C" void __cdecl VirtualdubFilterModuleDeinit(VDXFilterModule *fm, const VDXFilterFunctions *ff) {
}
