// VDXFrame - Helper library for VirtualDub plugins
//
// Copyright (C) 2008 Avery Lee
//
// SPDX-License-Identifier: Zlib
//

#ifndef f_VD2_VDXFRAME_UNKNOWN_H
#define f_VD2_VDXFRAME_UNKNOWN_H

#include <vd2/plugin/vdplugin.h>

extern "C" long _InterlockedExchangeAdd(volatile long *p, long v);
#pragma intrinsic(_InterlockedExchangeAdd)

template<class T> class vdxunknown : public T {
public:
	vdxunknown() : mRefCount(0) {}
	vdxunknown(const vdxunknown<T>& src) : mRefCount(0) {}		// do not copy the refcount
	virtual ~vdxunknown() {}

	vdxunknown<T>& operator=(const vdxunknown<T>&) {}			// do not copy the refcount

	virtual int VDXAPIENTRY AddRef() {
		return _InterlockedExchangeAdd(&mRefCount, 1) + 1;
	}

	virtual int VDXAPIENTRY Release() {
		long rc = _InterlockedExchangeAdd(&mRefCount, -1) - 1;
		if (!mRefCount) {
			mRefCount = 1;
			delete this;
			return 0;
		}

		return rc;
	}

	virtual void *VDXAPIENTRY AsInterface(uint32 iid) {
		if (iid == T::kIID)
			return static_cast<T *>(this);

		if (iid == IVDXUnknown::kIID)
			return static_cast<IVDXUnknown *>(this);

		return NULL;
	}

protected:
	volatile long	mRefCount;
};

template<class T1, class T2> class vdxunknown2 : public T1, public T2 {
public:
	vdxunknown2() : mRefCount(0) {}
	vdxunknown2(const vdxunknown2& src) : mRefCount(0) {}		// do not copy the refcount
	virtual ~vdxunknown2() {}

	vdxunknown2& operator=(const vdxunknown2&) {}				// do not copy the refcount

	virtual int VDXAPIENTRY AddRef() {
		return _InterlockedExchangeAdd(&mRefCount, 1) + 1;
	}

	virtual int VDXAPIENTRY Release() {
		long rc = _InterlockedExchangeAdd(&mRefCount, -1) - 1;
		if (!mRefCount) {
			mRefCount = 1;
			delete this;
			return 0;
		}

		return rc;
	}

	virtual void *VDXAPIENTRY AsInterface(uint32 iid) {
		if (iid == T1::kIID)
			return static_cast<T1 *>(this);

		if (iid == T2::kIID)
			return static_cast<T2 *>(this);

		if (iid == IVDXUnknown::kIID)
			return static_cast<IVDXUnknown *>(static_cast<T1 *>(this));

		return NULL;
	}

protected:
	volatile long	mRefCount;
};

#endif
