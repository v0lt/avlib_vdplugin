/*
 * Copyright (C) 2015-2020 Anton Shekhovtsov
 * Copyright (C) 2023-2025 v0lt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "../revision.h"

#ifndef REV_DATE
#define REV_DATE       0
#endif

#ifndef REV_HASH
#define REV_HASH       0
#endif

#ifndef REV_NUM
#define REV_NUM        0
#endif

#ifndef REV_BRANCH
#define REV_BRANCH     LOCAL
#endif

#define DO_MAKE_STR(x) #x
#define MAKE_STR(x)    DO_MAKE_STR(x)

#define VER_MAJOR      1
#define VER_MINOR      1
#define VER_BUILD      1

#define VERSION_NUM    VER_MAJOR,VER_MINOR,VER_BUILD,REV_NUM
#define VERSION_STR    MAKE_STR(VER_MAJOR) "." MAKE_STR(VER_MINOR) "." MAKE_STR(VER_BUILD) "." MAKE_STR(REV_NUM)

#define BRANCH_STR     MAKE_STR(REV_BRANCH)
