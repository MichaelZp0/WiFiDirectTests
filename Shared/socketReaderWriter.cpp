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
        unsigned int bytesRead = _socketReader.LoadAsync(sizeof(uint32_t)).get();
        if (bytesRead > 0)
        {
            unsigned int strLength = (unsigned int)_socketReader.ReadUInt32();
            try
            {
                bytesRead = _socketReader.LoadAsync(strLength).get();
                if (bytesRead > 0)
                {
                    std::wcout << "Got message: " << _socketReader.ReadString(strLength).c_str() << std::endl;
                    ReadMessage();
                }
                else
                {
                    std::wcout << "The remote side closed the socket" << std::endl;
                }
            }
            catch (std::exception& e)
            {
                std::wcout << "Failed to read from socket: " << e.what() << std::endl;
            }
        }
        else
        {
            std::wcout << "The remote side closed the socket" << std::endl;
        }
    }
    catch (std::exception& e)
    {
        std::wcout << "Failed to read from socket: " << e.what() << std::endl;
    }
}

void SocketReaderWriter::Close()
{
    _socketReader.Close();
    _socketWriter.Close();
    _streamSocket.Close();
}