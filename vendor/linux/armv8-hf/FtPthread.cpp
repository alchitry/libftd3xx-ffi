#include<iostream>
#include<cstdint>
#include<chrono>
#include<thread>
#include<future>
#include"USB_Device.h"

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#endif


//#define VERIFY_DATA

bool detect_key_press()
{
#ifdef _WIN32
	return GetAsyncKeyState(VK_ESCAPE) & 0x8001;
#else
	static auto key_detection_th = []()
	{
		int ch = 0;
		while (ch != 27)
		{
			struct termios oldattr, newattr;
			tcgetattr(STDIN_FILENO, &oldattr);
			newattr = oldattr;
			newattr.c_lflag &= ~(ICANON | ECHO);
			tcsetattr(STDIN_FILENO, TCSANOW, &newattr);
			ch = getchar();
			tcsetattr(STDIN_FILENO, TCSANOW, &oldattr);
		}
		return;
	};

	static bool threadIsActive = true;
	static auto m_future = std::async(std::launch::async, key_detection_th);
	if (!threadIsActive)
	{
		m_future = std::async(std::launch::async, key_detection_th);
		threadIsActive = true;
	}

	if (m_future.wait_for(std::chrono::microseconds(100)) == std::future_status::ready)
	{
		threadIsActive = false;
		return true;
	}
	return false;
#endif
}


void do_test(USB_Device* usb_device, int32_t device_sn, std::mutex &print_mtx)
{
	int32_t err;

	usb_device->startMeasurement();
	const uint32_t wait_data_cnt = 10000000;
	uint32_t buffer_size = wait_data_cnt;
	std::cout << "=========== Press ESC to exit! =========== \n";
	while (true)
	{
		if (detect_key_press())			//Exits the infinite loop, when pressing ESC button on the keyboard.
		{
			break;
		}
		buffer_size = wait_data_cnt;
		err = usb_device->waitForEvents(wait_data_cnt, 2000);
		if (err == USB_Device::STATUS_FATAL_ERROR)
		{
			print_mtx.lock();
			std::cout << "[S/N: " << device_sn <<  "]	" << "waitForEvents returned error:" << err << "\n";
			print_mtx.unlock();
			break;
		}
		else if (err == USB_Device::STATUS_WAIT_TIMEOUT)
		{
			buffer_size = usb_device->getBufferSize();
		}

		if(buffer_size == 0)
            continue;

		print_mtx.lock();
		std::cout << "[S/N: " << device_sn << "]	" << "Read data cnt : " << buffer_size << "\n";
		print_mtx.unlock();

		std::vector<uint32_t> data_vec;
		usb_device->read(data_vec, buffer_size);

#ifdef VERIFY_DATA
		uint32_t prev_value = data_vec[0];
		for (int i = 1; i < data_vec.size(); i++)
		{
			uint32_t value = data_vec[i];
			//std::cout << value << "     ";
			if ((prev_value + 1) != value)
				std::cout << "Error!\n";
			prev_value = value;
			//std::cout << "\n";
		}
#endif

	}
	usb_device->stopMeasurement();
}


int main()
{
	std::mutex print_mtx;
	std::vector<int32_t> deviceList;
	std::vector<std::thread> thread_vec;

	USB_Device::getDeviceList(deviceList);
	if (deviceList.empty())
	{
		std::cout << "Device not found\n";
		return 0;
	}

	std::cout << "Found " << deviceList.size() << " devices\n";

	std::vector<USB_Device*> usb_device_vec(deviceList.size());

	USB_Device::set_FT_Pipe_Configs();

	for (long unsigned int i = 0; i < deviceList.size(); ++i)
	{
		usb_device_vec[i] = new USB_Device();
		int32_t err = usb_device_vec[i]->openDevice(deviceList[i]);
		if (err != FT_OK )
		{
			std::cout << "usb_device  open fail! " << err << std::endl;
			return 0;
		}
	}

	for (long unsigned int i = 0; i < deviceList.size(); ++i)
	{
		thread_vec.push_back(std::thread(do_test, usb_device_vec[i], deviceList[i], std::ref(print_mtx)));
	}

	for (long unsigned int i = 0; i < deviceList.size(); ++i)
	{
		thread_vec[i].join();
		usb_device_vec[i]->closeDevice();
		delete usb_device_vec[i];
	}

	return 0;
}

