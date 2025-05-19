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

#pragma once

#include "pch.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Networking.Sockets.h>
#include <winrt/Windows.Storage.Streams.h>

class SocketReaderWriter
{
public:
    SocketReaderWriter(winrt::Windows::Networking::Sockets::StreamSocket socket, std::shared_ptr<bool> shouldClose) :
        _streamSocket(socket),
        _socketReader(winrt::Windows::Storage::Streams::DataReader(socket.InputStream())),
        _socketWriter(winrt::Windows::Storage::Streams::DataWriter(socket.OutputStream())),
        _shouldClose(shouldClose)
    {
        _socketReader.UnicodeEncoding(winrt::Windows::Storage::Streams::UnicodeEncoding::Utf8);
        _socketReader.ByteOrder(winrt::Windows::Storage::Streams::ByteOrder::LittleEndian);

        _socketWriter.UnicodeEncoding(winrt::Windows::Storage::Streams::UnicodeEncoding::Utf8);
        _socketWriter.ByteOrder(winrt::Windows::Storage::Streams::ByteOrder::LittleEndian);
    }

    void WriteMessage(winrt::hstring message);
    void ReadMessage();
    void Close();

private:
    winrt::Windows::Storage::Streams::DataReader _socketReader;
    winrt::Windows::Storage::Streams::DataWriter _socketWriter;
    winrt::Windows::Networking::Sockets::StreamSocket _streamSocket;

    std::shared_ptr<bool> _shouldClose;
};