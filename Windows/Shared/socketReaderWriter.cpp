//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "pch.h"
#include "SocketReaderWriter.h"

#include <iostream>
#include <cstdint>

#include <winrt/Windows.Devices.WiFiDirect.h>

using namespace winrt;
using namespace Windows::Networking::Sockets;
using namespace Windows::Storage::Streams;
using namespace Windows::Devices::WiFiDirect;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Foundation;

void SocketReaderWriter::WriteMessage(winrt::hstring message)
{
    _socketWriter.WriteUInt32(_socketWriter.MeasureString(message));
    _socketWriter.WriteString(message);

    try
    {
        unsigned int numBytesWritten = _socketWriter.StoreAsync().get();
        if (numBytesWritten > 0)
        {
            std::wcout << "Sent message: " << message.c_str() << std::endl;
        }
        else
        {
            std::wcout << "The remote side closed the socket." << std::endl;
        }
    }
    catch (std::exception& e)
    {
        std::wcout << "Failed to send message with error: " << e.what() << std::endl;
    }
}

void SocketReaderWriter::ReadMessage()
{
    try
    {
        DataReaderLoadOperation task = _socketReader.LoadAsync(sizeof(uint32_t));

		while (task.Status() != AsyncStatus::Completed)
		{
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            if (*_shouldClose)
            {
                return;
            }

            if (task.Status() == AsyncStatus::Canceled) {
                std::wcout << "Read operation was canceled." << std::endl;
            }

            if (task.Status() == AsyncStatus::Error) {
                std::wcout << "Read operation failed." << std::endl;
            }
		}

        unsigned int bytesInBuffer = task.get();
        if (bytesInBuffer > 0)
        {
            uint32_t readBytesOfStr = 0;
            uint32_t strLength = (unsigned int)_socketReader.ReadUInt32();

            if (strLength > 0)
            {
                std::wcout << "Got message: '";

                while (readBytesOfStr < strLength)
                {
                    try
                    {
                        uint32_t readBytes = _socketReader.LoadAsync(strLength).get();
						if (readBytes > 0)
						{
							readBytesOfStr += readBytes;
                            auto msg = _socketReader.ReadString(readBytes);
							std::wcout << msg.c_str();
						}
						else
						{
							std::wcout << "The remote side closed the socket" << std::endl;
							*_shouldClose = true;
                            break;
						}
                    }
                    catch (std::exception& e)
                    {
                        std::wcout << "Failed to read from socket: " << e.what() << std::endl;
                    }
                }

				std::wcout << "'" << std::endl;
            }

            
        }
        else
        {
            std::wcout << "The remote side closed the socket" << std::endl;
            *_shouldClose = true;
        }
    }
    catch (std::exception& e)
    {
        std::wcout << "Failed to read from socket: " << e.what() << std::endl;
        *_shouldClose = true;
    }
}

void SocketReaderWriter::Close()
{
    _socketReader.Close();
    _socketWriter.Close();
    _streamSocket.Close();
}