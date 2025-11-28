// VDXFrame - Helper library for VirtualDub plugins
//
// Copyright (C) 2008 Avery Lee
// Copyright (C) 2017 Anton Shekhovtsov
//
// SPDX-License-Identifier: Zlib
//

#ifndef f_VD2_VDXFRAME_VIDEOFILTERDIALOG_H
#define f_VD2_VDXFRAME_VIDEOFILTERDIALOG_H

#include <windows.h>

class VDXVideoFilterDialog {
public:
	VDXVideoFilterDialog();

protected:
	HWND	mhdlg;

	LRESULT Show(HINSTANCE hInst, LPCSTR templName, HWND parent);
	LRESULT Show(HINSTANCE hInst, LPCWSTR templName, HWND parent);
	HWND ShowModeless(HINSTANCE hInst, LPCSTR templName, HWND parent);
	HWND ShowModeless(HINSTANCE hInst, LPCWSTR templName, HWND parent);

	static INT_PTR CALLBACK StaticDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
	virtual INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
};

#endif
