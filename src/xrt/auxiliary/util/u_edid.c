// Copyright 2024, Gavin John
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief   Very simple EDID parsing functions.
 * @author  Gavin John <gavinnjohn@gmail.com>
 * @ingroup aux_util
 */

#include "u_logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <stdbool.h>

#define U_EDID_WARN(...) U_LOG_IFL_W(U_LOGGING_WARN, __VA_ARGS__)

struct u_edid
{
	// Generic EDID data
	char[4] manufacturer;
	uint16_t product;
};

void
u_edid_encode_manufacturer_id(uint16_t id, char *out)
{
	if (id >> 10 == 0 || (id >> 10) > 26 || (id >> 5) & 0x1F == 0 || ((id >> 5) & 0x1F) > 26 || id & 0x1F == 0 ||
	    (id & 0x1F) > 26) {
		out[0] = '\0';
		return;
	}
	out[0] = (id >> 10) + 'A' - 1;
	out[1] = ((id >> 5) & 0x1F) + 'A' - 1;
	out[2] = (id & 0x1F) + 'A' - 1;
	out[3] = '\0';
}

uint16_t
u_edid_decode_manufacturer_id(const char *in)
{
	if (strlen(in) != 3)
		return 0;
	// Convert to uppercase and check if it's a letter
	for (int i = 0; i < 3; i++) {
		if (in[i] >= 'a' && in[i] <= 'z') { // Convert to uppercase
			in[i] = in[i] - 'a' + 'A';
		} else if (in[i] < 'A' || in[i] > 'Z') { // Not a letter
			return 0;
		}
	}
	// Encode the manufacturer ID
	return ((in[0] - 'A' + 1) << 10) | ((in[1] - 'A' + 1) << 5) | (in[2] - 'A' + 1);
}

size_t
u_edid_get_num_displays(void)
{
	int fd = drmOpen("drm", NULL);
	if (fd < 0) {
		return 0;
	}

	drmModeRes *resources = drmModeGetResources(fd);
	if (resources == NULL) {
		drmClose(fd);
		return 0;
	}

	size_t num_displays = resources->count_connectors;
	drmModeFreeResources(resources);
	drmClose(fd);
	return num_displays;
}

void
u_edid_get_list(struct u_edid *out, size_t max_count, size_t *out_count)
{
	// Initialize count
	*out_count = 0;

	// Open the DRM device
	int fd = drmOpen("drm", NULL);
	if (fd < 0) {
		return;
	}

	// Get available resources
	drmModeRes *resources = drmModeGetResources(fd);
	if (resources == NULL) {
		drmClose(fd);
		*out_count = 0;
		return;
	}

	// Iterate through connectors
	for (int i = 0; i < resources->count_connectors; i++) {
		drmModeConnector *connector = drmModeGetConnector(fd, resources->connectors[i]);
		if (connector != NULL && connector->connection == DRM_MODE_CONNECTED) {
			drmModePropertyBlobPtr edid_blob = NULL;

			// Get EDID
			for (int j = 0; j < connector->count_props; j++) {
				drmModePropertyPtr property = drmModeGetProperty(fd, connector->props[j]);
				if (property && strcmp(property->name, "EDID") == 0) {
					edid_blob = drmModeGetPropertyBlob(fd, connector->prop_values[j]);
					drmModeFreeProperty(property);
					if (edid_blob && edid_blob->length >= 12) { // Ensure EDID blob is large enough
						const uint8_t *edid_data = edid_blob->data;
						uint16_t edid_manufacturer_id = (edid_data[8] << 8) | edid_data[9];
						uint16_t edid_product_id = (edid_data[10] << 8) | edid_data[11];

						// Add to list and ensure we don't overflow
						// Overflows should never happen, but better safe than sorry
						if (*out_count < max_count) {
							out[*out_count].manufacturer =
							    u_edid_encode_manufacturer_id(edid_manufacturer_id);
							out[*out_count].product = edid_product_id;
							(*out_count)++;
						} else {
							U_EDID_WARN("Overflow prevented in u_edid_get_list");
							U_EDID_WARN("This indicates a bug, please report it!");
						}
					}
					drmModeFreePropertyBlob(edid_blob);
					break;
				}
				drmModeFreeProperty(property);
			}
		}
		drmModeFreeConnector(connector);

		// If we have enough EDIDs, stop
		if (*out_count >= max_count) {
			U_EDID_WARN("EDID list overflowed, increase max_count");
			break;
		}
	}

	// Cleanup
	drmModeFreeResources(resources);
	drmClose(fd);
}

bool
u_edid_is_connected(const struct u_edid *edid)
{
	// Get list of EDIDs
	const size_t max_count = u_edid_get_num_displays();
	struct u_edid edids[max_count];
	size_t count = 0;
	u_edid_get_list(edids, max_count, &count);

	// Check if the EDID is in the list
	for (size_t i = 0; i < count; i++) {
		if (edid->manufacturer != NULL && strcmp(edid->manufacturer, edids[i].manufacturer) != 0) {
			continue;
		}
		if (edid->product != 0 && edid->product != edids[i].product) {
			continue;
		}
		return true;
	}
	return false;
}
