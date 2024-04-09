// Copyright 2024, Gavin John
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief   Very simple EDID parsing functions.
 * @author  Gavin John <gavinnjohn@gmail.com>
 * @ingroup aux_util
 */

#pragma once

#ifndef XRT_HAVE_LIBDRM
#error "This file should not be included if libdrm is not available."
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * Represents a single EDID.
 */
struct u_edid
{
	uint16_t manufacturer;
	uint16_t product;
};

/*!
 * Encode a manufacturer code.
 */
void
u_edid_encode_manufacturer_id(uint16_t id, char *out);

/*!
 * Decode a manufacturer code.
 */
uint16_t
u_edid_decode_manufacturer_id(const char *in);

/*!
 * Get the number of displays connected.
 */
size_t
u_edid_get_num_displays(void)

    /*!
     * Get a list of all the EDIDs.
     */
    void u_edid_get_list(struct u_edid *out, size_t max_count, size_t *out_count);

/*!
 * Get if an display with the given EDID data is connected.
 */
bool
u_edid_is_connected(const struct u_edid *edid);

#ifdef __cplusplus
}
#endif
