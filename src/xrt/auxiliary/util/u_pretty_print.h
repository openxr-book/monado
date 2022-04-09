// Copyright 2022, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Pretty printing various Monado things.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup aux_pretty
 */

#include "xrt/xrt_defines.h"


#ifdef __cplusplus
extern "C" {
#endif


/*!
 * @defgroup aux_pretty Pretty printing functions.
 *
 * Multiple modules implement additional pretty printing functions like
 * @ref st_oxr.
 *
 * Some functions have a `_indented` suffix added to them, this means that what
 * they print starts indented, but also they start with a newline. This is so
 * they can easily be chained together to form a debug message printing out
 * various information. Most the final logging functions in Monado inserts a
 * newline at the end of the message and we don't want two to be inserted.
 */

/*!
 * Function prototype for receiving pretty printed strings, do not keep a
 * reference to the pointer as it's often allocated on the stack for speed.
 *
 * @ingroup aux_pretty
 */
typedef void (*u_pp_delegate_func_t)(void *ptr, const char *str, size_t length);

/*!
 * Helper struct to hold a function pointer and data pointer.
 *
 * @ingroup aux_pretty
 */
struct u_pp_delegate
{
	//! Userdata pointer, placed first to match D/Volt delegates.
	void *ptr;

	//! String receiving function.
	u_pp_delegate_func_t func;
};

/*!
 * Helper typedef for delegate struct, less typing.
 *
 * @ingroup aux_pretty
 */
typedef struct u_pp_delegate u_pp_delegate_t;

/*!
 * Formats a string and sends to the delegate.
 *
 * @ingroup aux_pretty
 */
void
u_pp(struct u_pp_delegate dg, const char *fmt, ...) XRT_PRINTF_FORMAT(2, 3);

/*!
 * Pretty prints the @ref xrt_input_name.
 *
 * @ingroup aux_pretty
 */
void
u_pp_xrt_input_name(struct u_pp_delegate dg, enum xrt_input_name name);



#ifdef __cplusplus
}
#endif