// Copyright 2019-2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Does a probe and lets you call utility functions on devices.
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @author Jakob Bornecrantz <jakob@collabora.com>
 */


#include "xrt/xrt_instance.h"
#include "xrt/xrt_device.h"
#include "cli_common.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#define P(...) fprintf(stderr, __VA_ARGS__)

static int
cli_utility_print_help(int argc, const char **argv)
{
	P("Monado-CLI 0.0.1 - Utility Mode\n");
	P("Usage: %s utility [<DEVICENUM> <COMMANDNAME>]\n", argv[0]);
	P("\n");
	P("Running without passing a device number and command name\n");
	P("will list available devices and their commands.\n");

	return 1;
}

static int
do_exit(struct xrt_instance **xi_ptr, int ret)
{
	xrt_instance_destroy(xi_ptr);

	printf(" :: Exiting '%i'\n", ret);

	return ret;
}

#define NUM_XDEVS 32

static int
destroy_and_do_exit(struct xrt_instance **xi_ptr,
                    struct xrt_device *xdevs[NUM_XDEVS],
                    int ret)
{

	for (size_t i = 0; i < NUM_XDEVS; i++) {
		if (xdevs[i] == NULL) {
			continue;
		}
		xrt_device_destroy(&xdevs[i]);
	}
	return do_exit(xi_ptr, ret);
}

static void
cli_utility_print_device_commands(struct xrt_device *xdev)
{
	if (xdev->utility_methods == NULL) {
		printf("  No utility methods available.\n");
		return;
	}
	const struct xrt_device_utility_method_entry *entry =
	    xdev->utility_methods;
	printf("  Available utility methods:\n\n");
	while (entry->method_name[0] != '\0') {
		printf("  - %s\n", entry->method_name);
		entry++;
	}
}

static int
cli_utility_print_commands(struct xrt_device *xdevs[NUM_XDEVS])
{
	for (size_t i = 0; i < NUM_XDEVS; i++) {
		if (xdevs[i] == NULL) {
			continue;
		}
		printf("Device %d - '%s'\n", (int)i, xdevs[i]->str);
		cli_utility_print_device_commands(xdevs[i]);
		printf("\n\n");
	}
	return 0;
}

static int
cli_utility_invoke_command(struct xrt_device *xdev, const char *method_name)
{
	if (xdev->utility_methods == NULL) {
		P("  No utility methods available on the chosen device!\n");
		return 1;
	}
	const struct xrt_device_utility_method_entry *entry =
	    xdev->utility_methods;
	while (entry->method_name[0] != '\0') {
		if (0 == strncmp(entry->method_name, method_name,
		                 XRT_DEVICE_METHOD_NAME_LEN)) {
			printf("  - Executing %s\n", entry->method_name);
			int ret = xrt_device_invoke_utility_method(xdev, entry);
			printf("    Returned %d\n", ret);
			return ret;
		}
		entry++;
	}
	P("  No utility method named %s found on the chosen device!\n",
	  method_name);
	return 1;
}

int
cli_cmd_utility(int argc, const char **argv)
{
	struct xrt_device *xdevs[NUM_XDEVS] = {0};
	struct xrt_instance *xi = NULL;
	int ret = 0;

	if (argc != 2 && argc != 4) {
		return cli_utility_print_help(argc, argv);
	}

	// Initialize the prober.
	printf(" :: Creating instance!\n");

	ret = xrt_instance_create(NULL, &xi);
	if (ret != 0) {
		return do_exit(&xi, 0);
	}

	// Need to prime the prober with devices before dumping and
	// listing.
	printf(" :: Probing and selecting!\n");

	ret = xrt_instance_select(xi, xdevs, NUM_XDEVS);
	if (ret != 0) {
		return do_exit(&xi, ret);
	}

	if (argc == 2) {
		// print commands
		ret = cli_utility_print_commands(xdevs);
		if (ret != 0) {
			return destroy_and_do_exit(&xi, xdevs, ret);
		}
	}

	if (argc == 4) {
		int dev_num = atoi(argv[2]);
		if (dev_num < 0 || dev_num >= NUM_XDEVS) {
			P("Device number %d out of range! Should be a valid "
			  "device in 0 through %d\n\n",
			  dev_num, NUM_XDEVS - 1);
			cli_utility_print_help(argc, argv);
			return destroy_and_do_exit(&xi, xdevs, 1);
		}
		struct xrt_device *xdev = xdevs[dev_num];
		if (xdev == NULL) {

			P("No device number %d available.\n\n", dev_num);
			cli_utility_print_help(argc, argv);
			return destroy_and_do_exit(&xi, xdevs, 1);
		}
		ret = cli_utility_invoke_command(xdev, argv[3]);
	}


	// Finally done
	return destroy_and_do_exit(&xi, xdevs, ret);
}
