// Copyright 2022, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief Implement missing libc functions
 * @author happysmash27 <happysmash27@protonmail.com>
 * @ingroup aux_os
 */

#include "xrt/xrt_config_have.h"

#ifndef XRT_HAVE_SYSTEM_REALLOCARRAY

void *
reallocarray(void *optr, size_t nmemb, size_t size);

#endif
