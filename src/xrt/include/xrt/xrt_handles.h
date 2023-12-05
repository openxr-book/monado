// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Native handle types.
 * @author Ryan Pavlik <ryan.pavlik@collabora.com>
 * @ingroup xrt_iface
 */

#pragma once

#include "xrt_config_os.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef XRT_OS_WINDOWS
#include "xrt_windows.h"
#else // !XRT_OS_WINDOWS
#include "unistd.h"
#endif // !XRT_OS_WINDOWS


#ifdef __cplusplus
extern "C" {
#endif

#if defined(XRT_OS_WINDOWS)
/*!
 * The type for an IPC handle.
 *
 * On Windows, this is HANDLE.
 */
typedef HANDLE xrt_ipc_handle_t;

/*!
 * An invalid value for an IPC handle.
 *
 * Note that there may be more than one value that's invalid - use
 * @ref xrt_ipc_handle_is_valid instead of comparing against this!
 *
 * @relates xrt_ipc_handle_t
 */
#define XRT_IPC_HANDLE_INVALID INVALID_HANDLE_VALUE

/*!
 * Check whether an IPC handle is valid.
 *
 * @public @memberof xrt_ipc_handle_t
 */
static inline bool
xrt_ipc_handle_is_valid(xrt_ipc_handle_t handle)
{
	return handle != INVALID_HANDLE_VALUE;
}

/*!
 * Close an IPC handle.
 *
 * @public @memberof xrt_ipc_handle_t
 */
static inline void
xrt_ipc_handle_close(xrt_ipc_handle_t handle)
{
	CloseHandle(handle);
}

#else // !XRT_OS_WINDOWS

/*!
 * The type for an IPC handle.
 *
 * On non-Windows, this is a file descriptor.
 */
typedef int xrt_ipc_handle_t;

/*!
 * An invalid value for an IPC handle.
 *
 * Note that there may be more than one value that's invalid - use
 * @ref xrt_ipc_handle_is_valid instead of comparing against this!
 *
 * @relates xrt_ipc_handle_t
 */
#define XRT_IPC_HANDLE_INVALID (-1)

/*!
 * Check whether an IPC handle is valid.
 *
 * @public @memberof xrt_ipc_handle_t
 */
static inline bool
xrt_ipc_handle_is_valid(xrt_ipc_handle_t handle)
{
	return handle >= 0;
}

/*!
 * Close an IPC handle.
 *
 * @public @memberof xrt_ipc_handle_t
 */
static inline void
xrt_ipc_handle_close(xrt_ipc_handle_t handle)
{
	close(handle);
}

#endif // !XRT_OS_WINDOWS

/*
 *
 * xrt_shmem_handle_t
 *
 */

#if defined(XRT_OS_WINDOWS)
/*!
 * The type for shared memory blocks shared over IPC.
 *
 * On Windows, this is a HANDLE.
 */
typedef HANDLE xrt_shmem_handle_t;

/*!
 * Check whether a shared memory handle is valid.
 *
 * @public @memberof xrt_shmem_handle_t
 */
static inline bool
xrt_shmem_is_valid(xrt_shmem_handle_t handle)
{
	return handle != NULL;
}

/*!
 * An invalid value for a shared memory block.
 *
 * Note that there may be more than one value that's invalid - use
 * @ref xrt_shmem_is_valid instead of comparing against this!
 *
 * @relates xrt_shmem_handle_t
 */
#define XRT_SHMEM_HANDLE_INVALID (NULL)

#else // !XRT_OS_WINDOWS

/*!
 * The type for shared memory blocks shared over IPC.
 *
 * On Linux, this is a file descriptor.
 */
typedef int xrt_shmem_handle_t;

/*!
 * Defined to allow detection of the underlying type.
 *
 * @relates xrt_shmem_handle_t
 */
#define XRT_SHMEM_HANDLE_IS_FD 1

/*!
 * Check whether a shared memory handle is valid.
 *
 * @public @memberof xrt_shmem_handle_t
 */
static inline bool
xrt_shmem_is_valid(xrt_shmem_handle_t handle)
{
	return handle >= 0;
}

/*!
 * An invalid value for a shared memory block.
 *
 * Note that there may be more than one value that's invalid - use
 * @ref xrt_shmem_is_valid instead of comparing against this!
 *
 * @relates xrt_shmem_handle_t
 */
#define XRT_SHMEM_HANDLE_INVALID (-1)

#endif // !XRT_OS_WINDOWS

/*
 *
 * xrt_graphics_buffer_handle_t
 *
 */

/*!
 * @defgroup xrt_graphics_buffer_handle Graphics buffer native handle type
 * @ingroup xrt_iface
 *
 * Typedef, functions, and constants related to graphics buffer handles.
 *
 * @see xrt_graphics_sync_handle and @ref ipc-design
 * @{
 */

#if (defined(XRT_OS_ANDROID) && (__ANDROID_API__ >= 26)) || defined(XRT_DOXYGEN)

#ifndef XRT_DOXYGEN
typedef struct AHardwareBuffer AHardwareBuffer;
#endif

/*!
 * Defined if @ref xrt_graphics_buffer_handle_t is actually @p AHardwareBuffer* , to allow detection of the underlying
 * type and control implementation function selection.
 *
 * @see xrt_graphics_buffer_handle_t
 */
#define XRT_GRAPHICS_BUFFER_HANDLE_IS_AHARDWAREBUFFER 1

#endif


#if (defined(XRT_OS_LINUX) && !defined(XRT_GRAPHICS_BUFFER_HANDLE_IS_AHARDWAREBUFFER)) || defined(XRT_DOXYGEN)

/*!
 * Defined if @ref xrt_graphics_buffer_handle_t is actually a file descriptor (fd number), to allow detection of the
 * underlying type and control implementation function selection.
 *
 * @see xrt_graphics_buffer_handle_t
 */
#define XRT_GRAPHICS_BUFFER_HANDLE_IS_FD 1

#endif


#if defined(XRT_OS_WINDOWS) || defined(XRT_DOXYGEN)

/*!
 * Defined if @ref xrt_graphics_buffer_handle_t is actually a @p HANDLE , to allow detection of the underlying type and
 * control implementation function selection.
 *
 * @see xrt_graphics_buffer_handle_t
 */
#define XRT_GRAPHICS_BUFFER_HANDLE_IS_WIN32_HANDLE 1
#endif


/*!
 * @class xrt_graphics_buffer_handle_t
 *
 * The type underlying buffers shared between compositor clients and the main
 * compositor.
 *
 * - On Linux, this is a file descriptor.
 * - On Android platform 26+, this is an @p AHardwareBuffer pointer.
 * - On Windows, this is a @p HANDLE
 *
 * @see xrt_graphics_buffer_handle and @ref ipc-design
 */
typedef

#if defined(XRT_GRAPHICS_BUFFER_HANDLE_IS_FD)
    int

#elif defined(XRT_GRAPHICS_BUFFER_HANDLE_IS_AHARDWAREBUFFER)
    AHardwareBuffer *

#elif defined(XRT_GRAPHICS_BUFFER_HANDLE_IS_WIN32_HANDLE)
    HANDLE

#else
#error "xrt_graphics_buffer_handle_t not yet implemented for this platform"
#endif

        xrt_graphics_buffer_handle_t;


/*!
 * Check whether a graphics buffer handle is valid.
 *
 * @public @memberof xrt_graphics_buffer_handle_t
 */
static inline bool
xrt_graphics_buffer_is_valid(xrt_graphics_buffer_handle_t handle)
{

#if defined(XRT_GRAPHICS_BUFFER_HANDLE_IS_FD)
	return handle >= 0;

#elif defined(XRT_GRAPHICS_BUFFER_HANDLE_IS_AHARDWAREBUFFER)
	return handle != NULL;

#elif defined(XRT_GRAPHICS_BUFFER_HANDLE_IS_WIN32_HANDLE)
	return handle != NULL;

#else
#error "xrt_graphics_buffer_is_valid not yet implemented for this platform"
#endif
}


/*!
 * An invalid value for a graphics buffer.
 *
 * Note that there may be more than one value that's invalid - use
 * @ref xrt_graphics_buffer_is_valid() instead of comparing against this!
 *
 * @see xrt_graphics_buffer_handle_t
 */
#if defined(XRT_GRAPHICS_BUFFER_HANDLE_IS_FD)
#define XRT_GRAPHICS_BUFFER_HANDLE_INVALID (-1)

#elif defined(XRT_GRAPHICS_BUFFER_HANDLE_IS_AHARDWAREBUFFER)
#define XRT_GRAPHICS_BUFFER_HANDLE_INVALID NULL

#elif defined(XRT_GRAPHICS_BUFFER_HANDLE_IS_WIN32_HANDLE)
#define XRT_GRAPHICS_BUFFER_HANDLE_INVALID (NULL)

#else
#error "XRT_GRAPHICS_BUFFER_HANDLE_INVALID not yet implemented for this platform"
#endif

/*! @} */

/*
 *
 * xrt_graphics_sync_handle_t
 *
 */

/*!
 * @defgroup xrt_graphics_sync_handle Graphics sync native handle type
 * @ingroup xrt_iface
 *
 * Typedef, functions, and constants related to graphics sync handles.
 *
 * @see xrt_graphics_buffer_handle and @ref ipc-design
 * @{
 */

#if defined(XRT_OS_UNIX) || defined(XRT_DOXYGEN)

/*!
 * Defined if @ref xrt_graphics_sync_handle_t is actually a file descriptor, to allow detection of the
 * underlying type and control implementation function selection.
 *
 * @see xrt_graphics_sync_handle_t
 */
#define XRT_GRAPHICS_SYNC_HANDLE_IS_FD 1

#endif


#if defined(XRT_OS_WINDOWS) || defined(XRT_DOXYGEN)

/*!
 * Defined if @ref xrt_graphics_sync_handle_t is actually a @p HANDLE, to allow detection of the
 * underlying type and control implementation function selection.
 *
 * @see xrt_graphics_sync_handle_t
 */
#define XRT_GRAPHICS_SYNC_HANDLE_IS_WIN32_HANDLE 1

#endif


/*!
 * @class xrt_graphics_sync_handle_t
 *
 * The type underlying graphics synchronization primitives (semaphores, etc) shared
 * between compositor clients and the main compositor.
 *
 * - On Linux (including Android), this is a file descriptor.
 * - On Windows, this is a @p HANDLE.
 *
 * @see xrt_graphics_buffer_handle and @ref ipc-design
 */
typedef

#if defined(XRT_GRAPHICS_SYNC_HANDLE_IS_FD)
    int

#elif defined(XRT_GRAPHICS_SYNC_HANDLE_IS_WIN32_HANDLE)
    HANDLE

#else
#error "xrt_graphics_sync_handle_t not yet implemented for this platform"
#endif

        xrt_graphics_sync_handle_t;


/*!
 * Check whether a graphics sync handle is valid.
 *
 * @public @memberof xrt_graphics_sync_handle_t
 */
static inline bool
xrt_graphics_sync_handle_is_valid(xrt_graphics_sync_handle_t handle)
{

#if defined(XRT_GRAPHICS_SYNC_HANDLE_IS_FD)
	return handle >= 0;

#elif defined(XRT_GRAPHICS_SYNC_HANDLE_IS_WIN32_HANDLE)
	return handle != NULL;

#else
#error "xrt_graphics_sync_handle_is_valid not yet implemented for this platform"
#endif
}

/*!
 * An invalid value for a graphics sync primitive.
 *
 * Note that there may be more than one value that's invalid - use
 * xrt_graphics_sync_handle_is_valid() instead of comparing against this!
 *
 * @see xrt_graphics_sync_handle_t
 */
#if defined(XRT_GRAPHICS_SYNC_HANDLE_IS_FD)
#define XRT_GRAPHICS_SYNC_HANDLE_INVALID (-1)

#elif defined(XRT_GRAPHICS_SYNC_HANDLE_IS_WIN32_HANDLE)
#define XRT_GRAPHICS_SYNC_HANDLE_INVALID (NULL)

#else
#error "XRT_GRAPHICS_SYNC_HANDLE_INVALID not yet implemented for this platform"
#endif

/*! @} */

#ifdef __cplusplus
}
#endif
