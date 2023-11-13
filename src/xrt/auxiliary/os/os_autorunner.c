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

// Needed for determining which method of launching processes to use
#include "xrt/xrt_config_os.h"

//@todo Testing of whether the OS is POSIX-compliant or not should be done in xrt_config_os.h
#if __has_include(<unistd.h>)

// Needed for various functions and variables for launching processes
#include <unistd.h>

#if __has_include(<spawn.h>)
// Needed for `posix_spawn` functions
#include <spawn.h>
#endif

#endif


#include <stdlib.h>

static int
manage_autorun_process(pid_t pid, int pipe_filedes)
{

#if __has_include(<unistd.h>)

	// Pause this thread, with the pipe open, until further notice
	pause();

#endif

	return 0;
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

#if __has_include(<spawn.h>) && !defined(XRT_OS_ANDROID)

static void
autorunner_posix_spawn_error_handle_exit(struct os_thread_helper *managing_thread,
                                         int pipe_filedes[2],
                                         posix_spawn_file_actions_t *file_actions)
{
	// If appliciple, attempt to destroy posix_spawn file_actions object
	if (file_actions != NULL) {
		posix_spawn_file_actions_destroy(file_actions);
	}

	// If appliciple, close pipes
	if (pipe_filedes != NULL) {
		close(pipe_filedes[0]);
		close(pipe_filedes[1]);
	}

	// Close management thread
	os_thread_helper_signal_stop(managing_thread);
	pthread_exit((void *)-1);
}

#endif

void *
start_autorun_manage_thread(void *ptr)
{
	struct xrt_autorun *autorun = (struct xrt_autorun *)ptr;

	// Emulate what system() command does
	// but with everything explicit, so that functionality can be added, for, e.g
	//@todo Ability to auto-restart crashed processes in manage_autorun_process()

#ifdef XRT_OS_ANDROID

	// The functions in spawn.h are not implemneted on Android API versions under 28
	// Furthermore, it is preferred to:
	//@todo Implement native Android launch using JNI bridge
	U_LOG_E("Autorunner not yet implemented on Android");

#elif __has_include(<spawn.h>)

	// Launch code for posix-compliant systems with spawn.h

	// Set up exec, argv arguments
	char *exec = autorun->exec;
	size_t args_count = autorun->args_count;
	size_t argv_count = 1 + args_count;
	char *cmd_argv[argv_count + 1];
	concat_argv(cmd_argv, exec, autorun->args, args_count);

	// Logging info
	size_t command_str_buf_size = space_concat_str_array_buf_req(cmd_argv, argv_count);
	char command_str[command_str_buf_size];
	space_concat_str_array(command_str, cmd_argv, argv_count);

	// Return variables
	pid_t pid = -1;
	int launch_error = 0;
	int pipe_filedes[2];

	// Create pipe to use as stdin of child process
	// This enables us to stop it from stealing the stdin of the parent process,
	// While also avoiding the bug where `hello_xr` will immediately close
	// if a normal file (or anything else where `getchar` will return anything) is substituted as stdin instead.
	// Furthermore, it could be useful for automated interaction with child processes in the future
	launch_error = pipe(pipe_filedes);
	if (launch_error) {
		U_LOG_E("Error in creation of pipe");

		os_thread_helper_signal_stop(&autorun->managing_thread);
		pthread_exit((void *)-1);
	}

	// Initialise posix file actions object
	posix_spawn_file_actions_t file_actions;
	launch_error = posix_spawn_file_actions_init(&file_actions);
	if (launch_error) {
		U_LOG_E("Error in creation of posix file actions object");

		// Clean up and exit thread
		autorunner_posix_spawn_error_handle_exit(&autorun->managing_thread, pipe_filedes, NULL);
	}

	// Close unused write end of pipe
	launch_error = posix_spawn_file_actions_addclose(&file_actions, pipe_filedes[1]);
	if (launch_error) {
		U_LOG_E("Error in initialisation of posix file actions object");

		// Clean up and exit thread
		autorunner_posix_spawn_error_handle_exit(&autorun->managing_thread, pipe_filedes, &file_actions);
	}

	// Use read end of pipe as stdin
	launch_error = posix_spawn_file_actions_adddup2(&file_actions, pipe_filedes[0], STDIN_FILENO);
	if (launch_error) {
		U_LOG_E("Error in initialisation of posix file actions object");

		// Clean up and exit thread
		autorunner_posix_spawn_error_handle_exit(&autorun->managing_thread, pipe_filedes, &file_actions);
	}

	// Use posix_spawnp to spawn and execute child process
	U_LOG_I("Executing autorun process \"%s\"", command_str);
	launch_error = posix_spawnp(&pid, exec, &file_actions, NULL, cmd_argv, environ);
	if (launch_error) {
		// Error in `posix_spawnp`
		os_thread_helper_signal_stop(&autorun->managing_thread);
		pthread_exit((void *)-1);
	}

	// Clean up file_actions object
	posix_spawn_file_actions_destroy(&file_actions);
	// Close unused read end of pipe
	close(pipe_filedes[0]);

	// Parent process code
	long long int pm_exit_code = manage_autorun_process(pid, pipe_filedes[1]);

	// Once managing function exits, clean up and exit thread
	close(pipe_filedes[1]);
	os_thread_helper_signal_stop(&autorun->managing_thread);
	pthread_exit((void *)pm_exit_code);

#else
	// Emit error log if autorun management is not implemented for the OS
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
