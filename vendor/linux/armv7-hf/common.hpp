#ifndef COMMON_H_6J30YQFP
#define COMMON_H_6J30YQFP
#include <iostream>
#include <atomic>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include "ftd3xx.h"

using namespace std;

static bool do_exit;
static atomic_int tx_count;
static atomic_int rx_count;
static uint8_t in_ch_cnt;
static uint8_t out_ch_cnt;

static void sig_hdlr(int signum)
{
	switch (signum) {
	case SIGINT:
		do_exit = true;
		break;
	}
}

static void register_signals(void)
{
	signal(SIGINT, sig_hdlr);
}

static void get_version(void)
{
	DWORD dwVersion;

	FT_GetDriverVersion(NULL, &dwVersion);
	printf("Driver version:%d.%d.%d\r\n", dwVersion >> 24,
			(uint8_t)(dwVersion >> 16), dwVersion & 0xFFFF);

	FT_GetLibraryVersion(&dwVersion);
	printf("Library version:%d.%d.%d\r\n", dwVersion >> 24,
			(uint8_t)(dwVersion >> 16), dwVersion & 0xFFFF);
}

#if defined(_WIN32) || defined(_WIN64)
#define turn_off_thread_safe()
#else /* _WIN32 || _WIN64 */
static void turn_off_thread_safe(void)
{
	FT_TRANSFER_CONF conf;

	memset(&conf, 0, sizeof(FT_TRANSFER_CONF));
	conf.wStructSize = sizeof(FT_TRANSFER_CONF);
	conf.pipe[FT_PIPE_DIR_IN].fNonThreadSafeTransfer = true;
	conf.pipe[FT_PIPE_DIR_OUT].fNonThreadSafeTransfer = true;
	for (DWORD i = 0; i < 4; i++)
		FT_SetTransferParams(&conf, i);
}
#endif /* !_WIN32 && !_WIN64 */
static bool get_device_lists(int timeout_ms)
{
	DWORD count;
	FT_DEVICE_LIST_INFO_NODE nodes[16];

	chrono::steady_clock::time_point const timeout =
		chrono::steady_clock::now() +
		chrono::milliseconds(timeout_ms);

	do {
		if (FT_OK == FT_CreateDeviceInfoList(&count))
			break;
		this_thread::sleep_for(chrono::microseconds(10));
	} while (chrono::steady_clock::now() < timeout);
	printf("Total %u device(s)\r\n", count);
	if (!count)
		return false;

	if (FT_OK != FT_GetDeviceInfoList(nodes, &count))
		return false;
	return true;
}

static void show_throughput(FT_HANDLE handle)
{
	auto next = chrono::steady_clock::now() + chrono::seconds(1);;
	(void)handle;

	while (!do_exit) {
		this_thread::sleep_until(next);
		next += chrono::seconds(1);

		int tx = tx_count.exchange(0);
		int rx = rx_count.exchange(0);

		printf("TX:%.2fMiB/s RX:%.2fMiB/s, total:%.2fMiB\r\n",
			(float)tx/1000/1000, (float)rx/1000/1000,
			(float)(tx+ rx)/1000/1000);
	}
}

#endif /* end of include guard: COMMON_H_6J30YQFP */
