// Copyright 2023, Magic Leap, Inc.
// Copyright 2020, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Main file for Monado Windows service.
 * @author Julian Petrov <jpetrov@magicleap.com>
 * @ingroup ipc
 */

#include "xrt/xrt_config_os.h"
#include "server/ipc_server.h"
#include "server/ipc_server_interface.h"

#include "util/u_metrics.h"
#include "util/u_logging.h"
#include "util/u_debug.h"
#include "util/u_trace_marker.h"

#include "util/u_windows.h"
#include <winsvc.h>

#include <thread>
#include <mutex>

DEBUG_GET_ONCE_LOG_OPTION(log, "XRT_WINDOWS_SERVICE_LOG", U_LOGGING_INFO)
#define LOG_T(...) U_LOG_IFL_T(debug_get_log_option_log(), __VA_ARGS__)
#define LOG_D(...) U_LOG_IFL_D(debug_get_log_option_log(), __VA_ARGS__)
#define LOG_I(...) U_LOG_IFL_I(debug_get_log_option_log(), __VA_ARGS__)
#define LOG_W(...) U_LOG_IFL_W(debug_get_log_option_log(), __VA_ARGS__)
#define LOG_E(...) U_LOG_IFL_E(debug_get_log_option_log(), __VA_ARGS__)

class Service
{
	SERVICE_STATUS_HANDLE hstatus_ = {};
	HANDLE hevent_ = {};
	HANDLE hregistered_wait_ = {};
	std::thread monado_thread_;
	std::mutex mtx_;
	ipc_server *monado_server_ = nullptr;
	std::atomic_bool stopped_ = false;
	std::atomic_uint checkpoint_ = 0;

	void
	set_service_status(unsigned state, unsigned exit_code = NO_ERROR, unsigned wait_hint = 0)
	{
		SERVICE_STATUS status = {};
		status.dwServiceType = SERVICE_USER_OWN_PROCESS;
		status.dwCurrentState = state;
		status.dwWin32ExitCode = exit_code;
		status.dwWaitHint = wait_hint;

		switch (state) {
		default: break;
		case SERVICE_START_PENDING:
		case SERVICE_STOP_PENDING:
		case SERVICE_CONTINUE_PENDING:
		case SERVICE_PAUSE_PENDING: status.dwCheckPoint = ++checkpoint_; break;
		}
		if (state != SERVICE_START_PENDING) {
			status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
		}
		SetServiceStatus(hstatus_, &status);
	}

public:
	Service() = default;
	~Service()
	{
		LOG_D("(%p)", this);
		if (monado_thread_.joinable()) {
			monado_thread_.join();
		}
		set_service_status(SERVICE_STOPPED);
		UnregisterWait(hregistered_wait_);
		CloseHandle(hevent_);
	}
	void
	start(const char *service_name)
	{
		LOG_D("(%p): %s", this, service_name);
		hstatus_ = RegisterServiceCtrlHandlerExA(service_name, control_handler, this);
		set_service_status(SERVICE_START_PENDING);

		hevent_ = CreateEventW(nullptr, false, false, nullptr);
		RegisterWaitForSingleObject(&hregistered_wait_, hevent_, terminate, this, INFINITE, WT_EXECUTEDEFAULT);

		monado_thread_ = std::thread([this]() { ipc_server_main_windows_service(this); });
	}
	void
	stop()
	{
		if (!stopped_.exchange(true)) {
			set_service_status(SERVICE_STOP_PENDING);
			SetEvent(hevent_);
		}
	}
	void
	stop_monado_server()
	{
		std::lock_guard lock{mtx_};
		if (monado_server_) {
			ipc_server_handle_shutdown_signal(monado_server_);
		}
	}
	void
	set_monado_server(ipc_server *monado_server)
	{
		LOG_D("(%p): monado %p", this, monado_server);
		{
			std::lock_guard lock{mtx_};
			monado_server_ = monado_server;
		}
		if (monado_server) {
			set_service_status(SERVICE_RUNNING);
		} else {
			stop();
		}
	}
	DWORD
	control_handler(DWORD control, DWORD event_type, void *event_data)
	{
		LOG_D("(%p): ctrl %d, type %d, data %p", this, control, event_type, event_data);
		switch (control) {
		default: return ERROR_CALL_NOT_IMPLEMENTED;
		case SERVICE_CONTROL_INTERROGATE: return NO_ERROR;
		case SERVICE_CONTROL_STOP:
			stop_monado_server();
			stop();
			return NO_ERROR;
		}
	}
	static DWORD
	control_handler(DWORD control, DWORD event_type, void *event_data, void *context)
	{
		return static_cast<Service *>(context)->control_handler(control, event_type, event_data);
	}
	static void CALLBACK
	terminate(void *This, BOOLEAN timed_out)
	{
		delete static_cast<Service *>(This);
	}
};

void WINAPI
win32_service_main(DWORD dwNumServicesArgs, LPSTR *lpServiceArgVectors)
{
	LOG_D("%d service(s)", dwNumServicesArgs);
	for (unsigned i = 0; i < dwNumServicesArgs; i++) {
		LOG_D("%d. %s", i + 1, lpServiceArgVectors[i]);
	}
	(new Service())->start(lpServiceArgVectors[0]);
}

void
ipc_server_outer_set_server(void *svc, struct ipc_server *s)
{
	if (svc) {
		static_cast<Service *>(svc)->set_monado_server(s);
	}
}
