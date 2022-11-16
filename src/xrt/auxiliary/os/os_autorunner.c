// Copyright 2022, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief Autorunner functions (excluding loading JSON, which is in u_config_json.c)
 * @author happysmash27 <happysmash27@protonmail.com>
 * @ingroup aux_os
 */

#include "os/os_autorunner.h"
#include "util/u_logging.h"

#include "os/os_threading.h"

//@todo Testing of whether the OS is posix-compliant or not should be done in xrt_config_os.h
#if __has_include(<unistd.h>)
#include <unistd.h>
#endif

#include <stdlib.h>

static void
manage_autorun_process(pid_t pid)
{
	// Not implemented yet
	pthread_exit((void *)0);
}

// Calculate buffer requirements for space_concat_str_array()
static size_t
space_concat_str_array_buf_req(char **arr, size_t count)
{
	size_t len = 0;

	for (size_t i = 0; i < count; i++) {
		len += strlen(arr[i]) + 1;
	}

	return len;
}

/* Concatenate an array of strings into a single string, with a space at the end
   separating each string and a null byte at the end replacing the last space */
static void
space_concat_str_array(char *dest, char **arr, size_t count)
{
	size_t index = 0;
	size_t len = 0;

	for (size_t i = 0; i < count; i++) {
		len = strlen(arr[i]);
		U_LOG_D("Writing string %zu \"%s\" of size %zu to index %zu", i, arr[i], len, index);
		memcpy(&dest[index], arr[i], len);
		index += len;
		U_LOG_D("Writing a space to index %zu", index);
		dest[index] = ' ';
		index++;
	}

	// Overwrite last space terminating null byte
	U_LOG_D("Overwriting %zu with \\0", index - 1);
	dest[index - 1] = '\0';
}

// Combines an executable and an array of its arguments into an argv string array
// Note: char *dst[] needs to be one larger than args_count so the null terminator will fit
static void
concat_argv(char *dst[], char *exec, char **args, size_t args_count)
{
	dst[0] = exec;

	size_t i;
	for (i = 0; i < args_count; i++) {
		dst[i + 1] = args[i];
	}

	dst[i + 1] = NULL;
}

// Prints out argv in a similar way to how execv parses it
// Originally made to debug space_concat_str_array()
static void
debug_print_argv(char **argv)
{
	for (size_t i = 0; argv[i] != NULL; i++) {
		U_LOG_D("argv[%zu]: %s", i, argv[i]);
	}
}

void *
start_autorun_manage_thread(void *ptr)
{
	struct xrt_autorun *autorun = (struct xrt_autorun *)ptr;

	// Emulate what system() command does
	// but with everything explicit, so that functionality can be added, for, e.g
	//@todo Ability to auto-restart crashed processes in manage_autorun_process()

#if __has_include(<unistd.h>)

	pid_t pid = fork();
	if (pid == 0) {
		// Child process
		U_LOG_D("Child process launched");

		char *exec = autorun->exec;
		size_t args_count = autorun->args_count;
		size_t argv_count = 1 + args_count;
		char *cmd_argv[argv_count + 1];
		concat_argv(cmd_argv, exec, autorun->args, args_count);

		size_t command_str_buf_size = space_concat_str_array_buf_req(cmd_argv, argv_count);
		char command_str[command_str_buf_size];
		space_concat_str_array(command_str, cmd_argv, argv_count);

		U_LOG_I("Executing autorun process \"%s\"", command_str);
		execvp(exec, cmd_argv);
		os_thread_helper_signal_stop(&autorun->managing_thread);
		pthread_exit((void *)0);
	} else if (pid < 0) {
		// Error
		os_thread_helper_signal_stop(&autorun->managing_thread);
		pthread_exit((void *)-1);
	} else {
		// Parent process
		manage_autorun_process(pid);
		os_thread_helper_signal_stop(&autorun->managing_thread);
		pthread_exit((void *)0);
	}
#else
	U_LOG_E("Cannot start autorun management thread because this OS is not posix-compliant");
#endif
}

int
autorunner_start(struct xrt_autorunner *autorunner)
{
	size_t autorun_count = autorunner->autorun_count;
	U_LOG_I("Launching %zu autorun processes...", autorun_count);
	for (size_t i = 0; i < autorun_count; i++) {
		int ret;
		ret = os_thread_helper_init(&autorunner->autoruns[i].managing_thread);
		if (ret) {
			U_LOG_E("Failed to initialize the thread helper");
		}
		ret = os_thread_helper_start(&autorunner->autoruns[i].managing_thread, start_autorun_manage_thread,
		                             (void *)&autorunner->autoruns[i]);
		if (ret) {
			U_LOG_E("Failed to start autorunner managing thread %zu", i);
			return ret;
		}
	}
	return 0;
}

void
free_autorun_exec_args(struct xrt_autorun *autorun)
{
	// Free exec
	free(autorun->exec);

	// Free args
	char **args = autorun->args;
	size_t args_count = autorun->args_count;
	for (size_t i = 0; i < args_count; i++) {
		U_LOG_D("Freeing autorun arg %zu", i);
		free(args[i]);
	}
	free(args);
}

void
autorunner_destroy(struct xrt_autorunner *autorunner)
{
	size_t autorun_count = autorunner->autorun_count;
	// Note: For this to work properly before autorunner is initialised, the autorunner object must have been zeroed
	// out upon allocation. This is currently the case with the allocation of struct ipc_server in ipc_server_main()
	// in ipc_server_process.c, indirectly from which this function is expected to be used.
	for (size_t i = 0; i < autorun_count; i++) {
		os_thread_helper_destroy(&autorunner->autoruns[i].managing_thread);
		free_autorun_exec_args(&autorunner->autoruns[i]);
	}
	free(autorunner->autoruns);
}
