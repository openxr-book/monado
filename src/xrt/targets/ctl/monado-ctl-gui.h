// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Small cli application to control IPC service.
 * @author Pete Black <pblack@collabora.com>
 * @ingroup ipc
 */

#include "client/ipc_client.h"
#include "ipc_client_generated.h"

static int do_connect(struct ipc_connection *ipc_c);
int get_mode(struct ipc_connection *ipc_c);
int set_primary(struct ipc_connection *ipc_c, int client_id);
int set_focused(struct ipc_connection *ipc_c, int client_id);
int toggle_io(struct ipc_connection *ipc_c, int client_id);
int main_eel();

