// VDXFrame - Helper library for VirtualDub plugins
//
// Copyright (C) 2008 Avery Lee
// Copyright (C) 2017 Anton Shekhovtsov
//
// SPDX-License-Identifier: Zlib
//

#include "stdafx.h"
#include <vd2/VDXFrame/VideoFilterEntry.h>
#include <vd2/VDXFrame/VideoFilter.h>


VDXFilterDefinition2 *VDXGetVideoFilterDefinition(int index);

int VDXVideoFilterModuleInit2(struct VDXFilterModule *fm, const VDXFilterFunctions *ff, int vdfd_ver) {
  for(int i=0; ; ++i){
    VDXFilterDefinition2* def = VDXGetVideoFilterDefinition(i);
    if(!def) break;
    ff->addFilter(fm, def, sizeof(VDXFilterDefinition));
  }

  VDXVideoFilter::SetAPIVersion(vdfd_ver);
  
  return 0;
}

int VDXVideoFilterModuleInitFilterMod(struct VDXFilterModule *fm, const FilterModInitFunctions *ff, int vdfd_ver, int mod_ver) {
  for(int i=0; ; ++i){
    VDXFilterDefinition2* def = VDXGetVideoFilterDefinition(i);
    if(!def) break;
    ff->addFilter(fm, def, sizeof(VDXFilterDefinition), &def->filterMod, sizeof(FilterModDefinition));
  }

  VDXVideoFilter::SetAPIVersion(vdfd_ver);
  VDXVideoFilter::SetFilterModVersion(mod_ver);
  
  return 0;
}

