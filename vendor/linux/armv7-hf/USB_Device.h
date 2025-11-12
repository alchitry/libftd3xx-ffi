#pragma once
#include <iostream>
#include <cstdint>
#include <chrono>
#include <thread>
#include <deque>
#include <mutex>
#include <vector>
#include <utility>
#include <string>
#include <atomic>
#include <condition_variable>
#include <cstring>

#define FTD3XX_STATIC

#include "ftd3xx.h"
#include "Types.h"


class USB_Device
{
public:
	USB_Device();
	~USB_Device();

	enum
	{
		STATUS_OK = 0,
		STATUS_FATAL_ERROR = -1,
		STATUS_WAIT_TIMEOUT = 2
	};

	int32_t openDevice(int32_t serial_num);
	static int32_t getDeviceList(std::vector<int32_t> &serial_num_vec);
	static void set_FT_Pipe_Configs();


	uint32_t closeDevice();
	uint32_t startMeasurement();
	uint32_t stopMeasurement();

	int32_t waitForEvents(uint32_t count, uint32_t timeout_ms = 0);
	uint32_t read(std::vector<uint32_t>& data_vec, int count);
	uint32_t getBufferSize();

private:
	FT_HANDLE FT_handle;
	FT_STATUS ftStatus;
	uint8_t device_type;

#ifdef _WIN32
	const uint8_t READ_PIPE = 0x82;
	const uint8_t WRITE_PIPE = 0x02;
#else
	const uint8_t READ_PIPE = 0x00;
	const uint8_t WRITE_PIPE = 0x00;
#endif

	const ULONG ft_timeout = 2000;

	ULONG ft_bytes_transferred = 0;
	

	static const uint64_t k_buffer_size =16 *1024; //1048576;
	static const uint64_t k_buffer_cnt = 4095;

#ifdef _WIN32
	static const int k_overlapped_pipe_cnt = 32;
#else
	static const int k_overlapped_pipe_cnt = 4;		// Linux -> Max 8 async read per process. BUG?
#endif

	static const uint64_t k_usb_stream_buffer_size = k_buffer_cnt * k_buffer_size; // 4 GByte buffer
	static const int k_data_vec_size_step = 100000000;
	const int k_usb_stream_polling_rate_ms = 50;
	bool b_measurement_active = false;

	std::vector<uint32_t> data_vec;

	ULONG ulBytesTransferred[k_overlapped_pipe_cnt] = { 0 };
    OVERLAPPED vOverlapped[k_overlapped_pipe_cnt];
	

	std::deque<UCHAR*> buffer_pool;
	uint8_t* usb_stream_buffer;

	std::mutex usb_stream_mtx;
	std::mutex read_all_func_mtx;
    std::atomic<int32_t> lastError{0};
    std::atomic_bool b_stop_usb_stream_th{false};
	// std::atomic<int32_t> lastError = 0;
	// std::atomic_bool b_stop_usb_stream_th = false;
	std::condition_variable wait_event_func_cv;
	std::mutex wait_event_func_cv_mtx;
	std::atomic<uint32_t> data_vec_size_atm;

	std::thread usb_stream_th;
	std::thread data_processing_thread;

	std::deque<UCHAR*> busy_ptrs;
	std::deque<UCHAR*> out_ptrs;

	uint32_t clearFIFO();
	void abort_recovery();

	void usb_measurement_th();
	void usb_data_fetch_th();
};



