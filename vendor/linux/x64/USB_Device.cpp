#include "USB_Device.h"

void USB_Device::set_FT_Pipe_Configs()
{
#ifndef _WIN32
    FT_TRANSFER_CONF conf;
	memset(&conf, 0, sizeof(FT_TRANSFER_CONF));

	conf.wStructSize = sizeof(FT_TRANSFER_CONF);
	conf.pipe[FT_PIPE_DIR_IN].fNonThreadSafeTransfer = true;
	conf.pipe[FT_PIPE_DIR_IN].bURBCount = 32;
	conf.pipe[FT_PIPE_DIR_IN].dwURBBufferSize = k_buffer_size;
	conf.pipe[FT_PIPE_DIR_IN].dwStreamingSize = k_buffer_size;
	conf.pipe[FT_PIPE_DIR_OUT].fNonThreadSafeTransfer = true;
	FT_SetTransferParams(&conf, 0);
#endif
	return;
}


USB_Device::USB_Device()
{
	usb_stream_buffer = new uint8_t[k_usb_stream_buffer_size];
}

USB_Device::~USB_Device()
{
	delete[] usb_stream_buffer;
}

int32_t USB_Device::openDevice(int32_t serial_num)
{
	FT_HANDLE ftHandle;

	char serial_num_ch[16];
	std::sprintf(serial_num_ch, "%.12d", serial_num);

	ftStatus = FT_Create(serial_num_ch, FT_OPEN_BY_SERIAL_NUMBER, &ftHandle);

	if (FT_OK == ftStatus)
	{
		FT_handle = ftHandle;
		this->device_type = device_type;

		FT_EnableGPIO(FT_handle, 1, 1);
		FT_WriteGPIO(FT_handle, 1, 0);
		FT_SetPipeTimeout(FT_handle, READ_PIPE, ft_timeout);
		FT_SetPipeTimeout(FT_handle, WRITE_PIPE, ft_timeout);

		return 0;
	}
	else
		return ftStatus;
}

int32_t USB_Device::getDeviceList(std::vector<int32_t>& serial_num_vec)
{
	serial_num_vec.clear();

	FT_STATUS ftStatus;
	DWORD numDevs = 0;
	ftStatus = FT_CreateDeviceInfoList(&numDevs);
	if (!FT_FAILED(ftStatus) && numDevs > 0)
	{
		FT_DEVICE_LIST_INFO_NODE* devInfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(
			sizeof(FT_DEVICE_LIST_INFO_NODE) * numDevs);
		ftStatus = FT_GetDeviceInfoList(devInfo, &numDevs);
		if (!FT_FAILED(ftStatus))
		{
			for (DWORD i = 0; i < numDevs; i++)
			{
				if (!(devInfo[i].Flags & FT_FLAGS_OPENED) && !std::strcmp(devInfo[i].Description, "FTDI SuperSpeed-FIFO Bridge"))
				{
					serial_num_vec.push_back(std::stoi(devInfo[i].SerialNumber));
				}
			}
		}
		free(devInfo);
	}
	return ftStatus;
}

uint32_t USB_Device::closeDevice()
{
	if (FT_handle)
	{
		FT_Close(FT_handle);
		FT_handle = 0;
	}
	return 0;
}

uint32_t USB_Device::clearFIFO()
{
#ifdef _WIN32
	ULONG ulBytesTransferred;
	const int timeoutMs = 25;


	uint8_t flush_fifo_cmd[4] = { 0x00, 0x00, 0x00, 0x04};
	FT_WritePipeEx(FT_handle, WRITE_PIPE, (uint8_t*)flush_fifo_cmd, 4, &ulBytesTransferred, NULL);
	std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));

	//FT_SetPipeTimeout(FT_handle, 0x82, timeoutMs);
	abort_recovery();
	//FT_SetPipeTimeout(FT_handle, 0x82, ft_timeout);

	return 0;
#else
	FT_STATUS ftStatus;
	ULONG ulBytesTransferred;
	UCHAR data_bin[8];

	const int timeoutMs = 25;

    uint8_t flush_fifo_cmd[4] = { 0x00, 0x00, 0x00, 0x04};


	FT_WritePipeEx(FT_handle, WRITE_PIPE, (uint8_t*)flush_fifo_cmd, 4, &ulBytesTransferred, 0);

	FT_SetPipeTimeout(FT_handle, 0x82, timeoutMs);

	ftStatus = FT_ReadPipeEx(FT_handle, READ_PIPE, data_bin, 8, &ulBytesTransferred, 0);

	while (ftStatus == FT_OK)
	{
		ftStatus = FT_ReadPipeEx(FT_handle, READ_PIPE, data_bin, 8, &ulBytesTransferred, 0);
	}
	abort_recovery();
	FT_SetPipeTimeout(FT_handle, 0x82, ft_timeout);
	return 0;
#endif
}

void USB_Device::usb_measurement_th()
{
	int overlapped_index = 0;
	usb_stream_mtx.lock();
	for (uint64_t i = 0; i < k_buffer_cnt; ++i)
	{
		buffer_pool.push_back(&usb_stream_buffer[i * k_buffer_size]);
	}
	usb_stream_mtx.unlock();

	for (int i = 0; i < k_overlapped_pipe_cnt; i++)
	{
		ftStatus = FT_InitializeOverlapped(FT_handle, &vOverlapped[i]);
	}

	FT_SetStreamPipe(FT_handle, FALSE, FALSE, 0x82, k_buffer_size);
	FT_SetPipeTimeout(FT_handle, 0x82, ft_timeout);

	usb_stream_mtx.lock();
	for (int i = 0; i < k_overlapped_pipe_cnt; i++)
	{
#ifdef _WIN32
		ftStatus = FT_ReadPipeEx(FT_handle, READ_PIPE, buffer_pool.front(), k_buffer_size, &ulBytesTransferred[i], &vOverlapped[i]);
#else
		ftStatus = FT_ReadPipeAsync(FT_handle, READ_PIPE, buffer_pool.front(), k_buffer_size, &ulBytesTransferred[i], &vOverlapped[i]);
#endif
		busy_ptrs.push_back(buffer_pool.front());
		buffer_pool.pop_front();
	}
	usb_stream_mtx.unlock();

	uint8_t start_command[4] = { 0x00, 0x00, 0x00, 0x02 };
	FT_WritePipeEx(FT_handle, WRITE_PIPE, start_command, 4, &ft_bytes_transferred, 0);

    auto begin = std::chrono::steady_clock::now();

	while (!b_stop_usb_stream_th)
	{
		if (ftStatus != FT_IO_PENDING)
		{
			lastError = STATUS_FATAL_ERROR;
			std::cout << "FT error1: " <<  ftStatus << "\n";
			goto usb_stream_exit;
		}

        begin = std::chrono::steady_clock::now();
		while (ftStatus != FT_OK)
		{
			ftStatus = FT_GetOverlappedResult(FT_handle, &vOverlapped[overlapped_index], &ulBytesTransferred[overlapped_index], true);
			if (ftStatus == FT_IO_INCOMPLETE)
			{
                if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count() >= ft_timeout * 3)
				{

					std::cout << "Infinite FT_IO_INCOMPLETE state detected\n";
					std::this_thread::sleep_for(std::chrono::seconds(2));
					//lastError = STATUS_FATAL_ERROR;
					//goto usb_stream_exit;
				}
			}
			else if (ftStatus == FT_TIMEOUT)
			{
				lastError = STATUS_FATAL_ERROR;
				std::cout << "FT error2: " << ftStatus << "\n";
				goto usb_stream_exit;
			}
			else if ((ftStatus != FT_OK))
			{
				std::cout << "FT error3: " << ftStatus << "\n";
				lastError = STATUS_FATAL_ERROR;
				goto usb_stream_exit;
			}
			std::this_thread::sleep_for(std::chrono::nanoseconds(10));
		}

		if (ulBytesTransferred[overlapped_index] != k_buffer_size)
		{
			lastError = STATUS_FATAL_ERROR;
			goto usb_stream_exit;
		}

		usb_stream_mtx.lock();
		out_ptrs.push_back(busy_ptrs.front());
		busy_ptrs.pop_front();

		if (buffer_pool.size() == 0)
		{
			lastError = STATUS_FATAL_ERROR;
			std::cout << "Buffer full\n";
			usb_stream_mtx.unlock();
			goto usb_stream_exit;
		}

		UCHAR* buffer_front = buffer_pool.front();
#ifdef _WIN32
		ftStatus = FT_ReadPipeEx(FT_handle, READ_PIPE, buffer_front, k_buffer_size, &ulBytesTransferred[overlapped_index], &vOverlapped[overlapped_index]);
#else
		ftStatus = FT_ReadPipeAsync(FT_handle, READ_PIPE, buffer_front, k_buffer_size, &ulBytesTransferred[overlapped_index], &vOverlapped[overlapped_index]);
#endif
		busy_ptrs.push_back(buffer_pool.front());
		buffer_pool.pop_front();
		usb_stream_mtx.unlock();

		if (++overlapped_index == k_overlapped_pipe_cnt)
		{
			overlapped_index = 0;
		}
	}

usb_stream_exit:

	uint8_t stop_command[4] = { 0x00, 0x00, 0x00, 0x03};
	FT_WritePipeEx(FT_handle, WRITE_PIPE, stop_command, 4, &ft_bytes_transferred, 0);
	abort_recovery();

	for (int i = 0; i < k_overlapped_pipe_cnt; i++)
	{
		ftStatus = FT_ReleaseOverlapped(FT_handle, &vOverlapped[i]);
	}
	ftStatus = FT_ClearStreamPipe(FT_handle, FALSE, FALSE, 0x82);
	clearFIFO();

	//wait_event_func_cv.notify_one();
	return;
}

void USB_Device::usb_data_fetch_th()
{
	std::deque<UCHAR*> data;
	while (!b_stop_usb_stream_th)
	{
		usb_stream_mtx.lock();
		data = std::move(out_ptrs);
		out_ptrs.clear();
		usb_stream_mtx.unlock();

		auto begin = std::chrono::steady_clock::now();
		if (data.size() != 0)
		{
			uint64_t expected_max_size = data.size() * k_buffer_size / 4;

			read_all_func_mtx.lock();
			//auto start_1 = std::chrono::steady_clock::now();
			uint64_t timetag_vector_free_space = data_vec.capacity() - data_vec.size();
			if ((long)(timetag_vector_free_space) - (long)(expected_max_size) < 0)
			{
				try
				{
					data_vec.reserve(data_vec.size() + k_data_vec_size_step);
				}
				catch (std::exception const&)
				{
					lastError = STATUS_FATAL_ERROR;
					b_stop_usb_stream_th = true;
					read_all_func_mtx.unlock();
					break;
				}
			}

			for (long unsigned int i = 0; i < data.size(); ++i)
			{
				uint8_t* raw_data_frame = data[i];
				for (long unsigned int j = 0; j < k_buffer_size; j += 4)
				{
					uint32_t data = *((uint32_t*)&raw_data_frame[j]);
					data_vec.push_back(data);
				}
			}

			read_all_func_mtx.unlock();


			data_vec_size_atm.store(data_vec.size());
			wait_event_func_cv.notify_one();

			usb_stream_mtx.lock();
			for (long unsigned int i = 0; i < data.size(); ++i)
			{
				//buffer_pool.push_back(data[i]);
				buffer_pool.push_front(data[i]);
			}
			usb_stream_mtx.unlock();
		}

		int elapsed_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
		if (elapsed_time_ms < k_usb_stream_polling_rate_ms)
			std::this_thread::sleep_for(std::chrono::milliseconds(k_usb_stream_polling_rate_ms - elapsed_time_ms));

	}
	data.clear();
	return;
}

uint32_t USB_Device::startMeasurement()
{
	lastError = 0;
	if (!b_measurement_active)
	{
		data_vec.clear();
		data_vec_size_atm = 0;
		b_stop_usb_stream_th = false;
		b_measurement_active = true;
		usb_stream_th = std::thread(&USB_Device::usb_measurement_th, this);
		data_processing_thread = std::thread(&USB_Device::usb_data_fetch_th, this);
	}
	else
	{
		return STATUS_FATAL_ERROR;
	}
	return STATUS_OK;
}

uint32_t USB_Device::stopMeasurement()
{
	if (b_measurement_active)
	{
		b_measurement_active = false;

		b_stop_usb_stream_th = true;
		usb_stream_th.join();
		data_processing_thread.join();
		b_stop_usb_stream_th = false;
		wait_event_func_cv.notify_one();
		buffer_pool.clear();
		busy_ptrs.clear();
		out_ptrs.clear();
	}
	return 0;
}

int32_t USB_Device::waitForEvents(uint32_t count, uint32_t timeout_ms)
{
	int32_t returnCode = STATUS_OK;
	auto predicate = [&]
	{
		return (data_vec_size_atm.load() >= count) || !b_measurement_active || lastError;
	};
	if (b_measurement_active)
	{
		std::unique_lock<std::mutex> ul(wait_event_func_cv_mtx);
		if (timeout_ms > 0)
		{
			if (!wait_event_func_cv.wait_for(ul, std::chrono::milliseconds(timeout_ms), predicate))
			{
				returnCode = STATUS_WAIT_TIMEOUT;
				lastError = 0;
			}
		}
		else
			wait_event_func_cv.wait(ul, predicate);

		//std::cout << "Atomic: " << timetag_vec_size_atm << std::endl;
		//timetag_vec_size_atm = 0;
	}
	if (lastError)
	{
		returnCode = STATUS_FATAL_ERROR;
		lastError = 0;
	}

	return returnCode;
}

uint32_t USB_Device::read(std::vector<uint32_t>& data_vec, int count)
{
	data_vec.resize(count);
	read_all_func_mtx.lock();
	auto begin_it = this->data_vec.begin();
	auto end_it = this->data_vec.begin() + count;

	for (int i = 0; i < count; ++i)
		data_vec[i] = this->data_vec[i];

	this->data_vec.erase(begin_it, end_it);
	data_vec_size_atm = this->data_vec.size();
	read_all_func_mtx.unlock();
	return 0;
}

uint32_t USB_Device::getBufferSize()
{
	return data_vec_size_atm;
}

void USB_Device::abort_recovery()
{
	UCHAR ucDirection = (FT_GPIO_DIRECTION_OUT << FT_GPIO_0) ;
	UCHAR ucMask = (FT_GPIO_VALUE_HIGH << FT_GPIO_0);
	ftStatus = FT_EnableGPIO(FT_handle, ucMask, ucDirection);
    if(ftStatus != FT_OK)
	{
		printf("FT_EnableGPIO is Failed...=%d\n",ftStatus);
		return ;
	}
	ftStatus = FT_WriteGPIO(FT_handle,ucMask, (FT_GPIO_VALUE_HIGH << FT_GPIO_0));
	    if(ftStatus != FT_OK)
	{
		printf("FT_WriteGPIO is Failed...=%d\n",ftStatus);
		return ;
	}
	FT_AbortPipe(FT_handle,0x82);
	std::this_thread::sleep_for(std::chrono::milliseconds(25));	
	FT_WriteGPIO(FT_handle,ucMask, (FT_GPIO_VALUE_LOW << FT_GPIO_0));
}

