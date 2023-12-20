// Copyright 2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief A implementation of the @ref xrt_space_overseer interface.
 *
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup aux_util
 */

#include "xrt/xrt_space.h"


#ifdef __cplusplus
extern "C" {
#endif


struct xrt_session_event_sink;


/*
 *
 * Structs and defines.
 *
 */

/*!
 * Keeps track of what kind of space it is.
 */
enum u_space_type
{
	U_SPACE_TYPE_NULL,
	U_SPACE_TYPE_POSE,
	U_SPACE_TYPE_OFFSET,
	U_SPACE_TYPE_ROOT,
};

/*!
 * Representing a single space, can be several ones. There should only be one
 * root space per overseer.
 */
struct u_space
{
	struct xrt_space base;

	/*!
	 * The space this space is in.
	 */
	struct u_space *next;

	/*!
	 * The type of the space.
	 */
	enum u_space_type type;

	union {
		struct
		{
			struct xrt_device *xdev;
			enum xrt_input_name xname;
		} pose;

		struct
		{
			struct xrt_pose pose;
		} offset;
	};
};


/*
 *
 * Helper functions.
 *
 */

static inline struct u_space *
u_space(struct xrt_space *xs)
{
	return (struct u_space *)xs;
}


/*
 *
 * Main interface.
 *
 */

/*!
 * Create a default implementation of a space overseer.
 *
 * @param[in] broadcast Event sink that broadcasts events to all sessions.
 * @ingroup aux_util
 */
struct u_space_overseer *
u_space_overseer_create(struct xrt_session_event_sink *broadcast);

/*!
 * Sets up the space overseer and all semantic spaces in a way that works with
 * the old @ref xrt_tracking_origin information. Will automatically create local
 * and stage spaces. If another setup is needed the builder should manually set
 * the space graph up using below functions.
 *
 * @ingroup aux_util
 */
void
u_space_overseer_legacy_setup(struct u_space_overseer *uso,
                              struct xrt_device **xdevs,
                              uint32_t xdev_count,
                              struct xrt_device *head,
                              const struct xrt_pose *local_offset,
                              bool root_is_unbounded);

/*!
 * Creates a space without any offset, this is just for optimisation over a
 * regular offset space.
 *
 * @ingroup aux_util
 */
void
u_space_overseer_create_null_space(struct u_space_overseer *uso,
                                   struct xrt_space *parent,
                                   struct xrt_space **out_space);

/*!
 * The space overseer internally keeps track the space that @ref xrt_device is
 * in, and then uses that mapping when creating pose spaces. This function
 * allows builders to create a much more bespoke setup. This function adds a
 * reference to the space.
 *
 * @ingroup aux_util
 */
void
u_space_overseer_link_space_to_device(struct u_space_overseer *uso, struct xrt_space *xs, struct xrt_device *xdev);


/*
 *
 * Builder helpers.
 *
 */

/*!
 * @copydoc xrt_space_overseer::create_offset_space
 *
 * Convenience helper for builder code using @ref u_space_overseer directly.
 *
 * @public @memberof u_space_overseer
 */
static inline xrt_result_t
u_space_overseer_create_offset_space(struct u_space_overseer *uso,
                                     struct xrt_space *parent,
                                     const struct xrt_pose *offset,
                                     struct xrt_space **out_space)
{
	struct xrt_space_overseer *xso = (struct xrt_space_overseer *)uso;
	return xrt_space_overseer_create_offset_space(xso, parent, offset, out_space);
}

/*!
 * @copydoc xrt_space_overseer::create_pose_space
 *
 * Convenience helper for builder code using @ref u_space_overseer directly.
 *
 * @public @memberof u_space_overseer
 */
static inline xrt_result_t
u_space_overseer_create_pose_space(struct u_space_overseer *uso,
                                   struct xrt_device *xdev,
                                   enum xrt_input_name name,
                                   struct xrt_space **out_space)
{
	struct xrt_space_overseer *xso = (struct xrt_space_overseer *)uso;
	return xrt_space_overseer_create_pose_space(xso, xdev, name, out_space);
}


#ifdef __cplusplus
}
#endif
