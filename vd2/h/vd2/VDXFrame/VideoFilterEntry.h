// VDXFrame - Helper library for VirtualDub plugins
//
// Copyright (C) 2008 Avery Lee
// Copyright (C) 2017 Anton Shekhovtsov
//
// SPDX-License-Identifier: Zlib
//

#ifndef f_VD2_VDXFRAME_VIDEOFILTERENTRY_H
#define f_VD2_VDXFRAME_VIDEOFILTERENTRY_H

struct VDXFilterModule;
struct VDXFilterFunctions;
struct FilterModInitFunctions;
struct VDXFilterDefinition2;

///////////////////////////////////////////////////////////////////////////////
/// Video filter declaration macros
///
/// To declare video filters, use the following pattern:
///
///	VDX_DECLARE_VIDEOFILTERS_BEGIN()
///		VDX_DECLARE_VIDEOFILTER(definitionSymbolName)
///	VDX_DECLARE_VIDEOFILTERS_END()
///
/// Each entry points to a variable of type VDXFilterDefinition. Note that these must
/// be declared as _non-const_ for compatibility with older versions of VirtualDub.
/// Video filters declared this way are automatically registered by the module init
/// routine.
///
#define VDX_DECLARE_VIDEOFILTERS_BEGIN()		VDXFilterDefinition2 *VDXGetVideoFilterDefinition(int index) {
#define VDX_DECLARE_VIDEOFILTER(defVarName)			if (!index--) { extern VDXFilterDefinition2 defVarName; return &defVarName; }
#define VDX_DECLARE_VIDEOFILTERS_END()				return NULL;	\
												}
#define VDX_DECLARE_VFMODULE()

int VDXVideoFilterModuleInit2(struct VDXFilterModule *fm, const VDXFilterFunctions *ff, int vdfd_ver);
int VDXVideoFilterModuleInitFilterMod(struct VDXFilterModule *fm, const FilterModInitFunctions *ff, int vdfd_ver, int mod_ver);

#endif
