// Copyright 2022, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief Header for loading autorunner
 * @author happysmash27 <happysmash27@protonmail.com>
 * @ingroup aux_os
 */

#pragma once

#include "os/os_threading.h"

struct xrt_autorun
{
	char *exec;
	char **args;
	size_t args_count;

	struct os_thread_helper managing_thread;
};

struct xrt_autorunner
{
	struct xrt_autorun *autoruns;
	size_t autorun_count;
};

void *
start_autorun_manage_thread(void *ptr);

int
autorunner_start(struct xrt_autorunner *autorunner);

void
free_autorun_exec_args(struct xrt_autorun *autorun);

void
autorunner_destroy(struct xrt_autorunner *autorunner);
