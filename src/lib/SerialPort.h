// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2022 Julio Cesar Ziviani Alvarez

#pragma once

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/serial.h>

#define HANDLE int
#define INVALID_HANDLE_VALUE (-1)
#else
#error Operating System not supported
#endif

#include "Result.h"

namespace makeland {
    enum class Parity {
        None,
        Odd,
        Even,
        Mark,
        Space
    };

    enum class StopBits
    {
        One,
        Two,
        OnePointFive
    };

    enum class BaudRate
    {
        _1200,
        _2400,
        _4800,
        _9600,
        _19200,
        _38400,
        _57600,
        _115200,
        _230400
    };

    class SerialPort {
    public:
        SerialPort() = default;
        SerialPort(const SerialPort&) = delete;
        SerialPort& operator=(const SerialPort&) = delete;
        SerialPort(const SerialPort&&) = delete;
        SerialPort& operator=(const SerialPort&&) = delete;
        ~SerialPort() = default;

        bool isOpened() {
            if (handle == INVALID_HANDLE_VALUE) {
                return false;
            }
            return true;
        }

        Result open(string portName, BaudRate baudRate, Parity parity, int dataBits, StopBits stopBits) {
            if (isOpened()) {
                return Result::ok();
            }
#ifdef _WIN32
            handle = CreateFileA(portName.data(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                0,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                0);

            if (handle == INVALID_HANDLE_VALUE) {
                return Result("serial_port_error", "Error opening serial port {}: {}", portName, getLastError());
            }

            if (PurgeComm(handle, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR) == 0) {
                return Result("serial_port_error", "Serial port error calling method `PurgeComm`: {}", getLastError());
            }

            DCB dcb;

            if (!GetCommState(handle, &dcb)) {
                return Result("serial_port_error", "Serial port error calling method `GetCommState`: {}", getLastError());
            }
            int baud;

            switch (baudRate) {
                case BaudRate::_1200: baud = 1200;
                    break;

                case BaudRate::_2400: baud = 2400;
                    break;

                case BaudRate::_4800: baud = 4800;
                    break;

                case BaudRate::_9600: baud = 9600;
                    break;

                case BaudRate::_19200: baud = 19200;
                    break;

                case BaudRate::_38400: baud = 38400;
                    break;

                case BaudRate::_57600: baud = 57600;
                    break;

                case BaudRate::_115200: baud = 115200;
                    break;

                case BaudRate::_230400: baud = 230400;
                    break;
            }

            switch (parity) {
                case Parity::None: dcb.Parity = NOPARITY;
                    break;

                case Parity::Odd: dcb.Parity = ODDPARITY;
                    break;

                case Parity::Even: dcb.Parity = EVENPARITY;
                    break;

                case Parity::Mark: dcb.Parity = MARKPARITY;
                    break;

                case Parity::Space: dcb.Parity = SPACEPARITY;
                    break;
            };

            switch (stopBits) {
                case StopBits::One: dcb.StopBits = ONESTOPBIT;
                    break;

                case StopBits::Two: dcb.StopBits = TWOSTOPBITS;
                    break;

                case StopBits::OnePointFive: dcb.StopBits = ONE5STOPBITS;
                    break;
            };
            dcb.BaudRate = baud;
            dcb.ByteSize = (BYTE)dataBits;

            dcb.fOutxCtsFlow = false;
            dcb.fOutxDsrFlow = false;
            dcb.fOutX = false;
            dcb.fDtrControl = DTR_CONTROL_DISABLE;
            dcb.fRtsControl = RTS_CONTROL_DISABLE;

            if (!SetCommState(handle, &dcb)) {
                return Result("serial_port_error", "Serial port error calling method `SetCommState`: {}", getLastError());
            }
#elif __linux__
            struct termios settings;
            memset(&settings, 0, sizeof(settings));
            settings.c_iflag = 0;
            settings.c_oflag = 0;

            settings.c_cflag = CREAD | CLOCAL;

            switch (dataBits) {
                case 5: settings.c_cflag |= CS5;
                    break;

                case 6: settings.c_cflag |= CS6;
                    break;

                case 7: settings.c_cflag |= CS7;
                    break;

                default:settings.c_cflag |= CS8;
                    break;
            }

            switch (parity) {
                case Parity::Odd: settings.c_cflag = PARENB | PARODD;
                    break;

                case Parity::Even: settings.c_cflag = PARENB;
                    break;

                default:
                    break;
            };

            speed_t baud;

            switch (baudRate) {
                case BaudRate::_1200: baud = B1200;
                    break;
                case BaudRate::_2400: baud = B2400;
                    break;
                case BaudRate::_4800: baud = B4800;
                    break;
                case BaudRate::_9600: baud = B9600;
                    break;
                case BaudRate::_19200: baud = B19200;
                    break;
                case BaudRate::_38400: baud = B38400;
                    break;
                case BaudRate::_57600: baud = B57600;
                    break;
                case BaudRate::_115200: baud = B115200;
                    break;
                case BaudRate::_230400: baud = B230400;
                    break;
            }

            settings.c_lflag = 0;
            settings.c_cc[VMIN] = 1;
            settings.c_cc[VTIME] = 0;

            handle = ::open(portName.data(), O_RDWR);

            if (handle == -1) {
                return Result("serial_port_error", "Serial port error calling method `open`: {}", getLastError());
            }

            cfsetospeed(&settings, baud);
            cfsetispeed(&settings, baud);
            tcsetattr(handle, TCSANOW, &settings);
#endif
            return Result::ok();
        }

        Result close() {
            if (!isOpened()) {
                return Result::ok();
            }
#ifdef _WIN32
            CloseHandle(handle);
#elif __linux__
            ::close(handle);
#endif
            handle = INVALID_HANDLE_VALUE;

            return Result::ok();
        }

        Result read(uint8_t* buffer, size_t size, size_t* totalRead) {
            if (!isOpened()) {
                return Result("serial_port_not_opened", "Serial port is not Opened");
            }
#ifdef _WIN32
            DWORD numberOfBytesRead;

            if (!ReadFile(handle, buffer, (DWORD)size, &numberOfBytesRead, NULL)) {
                return Result("serial_port_error", "Serial port error calling method `ReadFile`: {}", getLastError());
            }
            if (totalRead) {
                *totalRead = numberOfBytesRead;
            }
#elif __linux__
            int count = ::read(handle, (void*)buffer, size);

            if (count == -1) {
                return Result("serial_port_error", "Serial port error calling method `read`: {}", getLastError());
            }
            if (totalRead) {
                *totalRead = (size_t)count;
            }
#endif
            return Result::ok();
        }

        Result write(uint8_t* buffer, size_t size) {
            if (!isOpened()) {
                return Result("serial_port_not_opened", "Serial port is not Opened");
            }
#ifdef _WIN32
            DWORD written;

            if (!WriteFile(handle, buffer, (DWORD)size, &written, NULL)) {
                return Result("serial_port_error", "Serial port error calling method `WriteFile`: {}", getLastError());
            }
            if (written != size) {
                return Result("not_written", "Not all data written in com port");
            }
#elif __linux__
            int count = ::write(handle, (void*)buffer, size);

            if (count == -1) {
                return Result("serial_port_error", "Serial port error calling method `write`: {}", getLastError());
            }
            if ((size_t)count != size) {
                return Result("not_written", "Not all data written in com port");
            }
#endif
            return Result::ok();
        }

    private:
        HANDLE handle = INVALID_HANDLE_VALUE;

        string getLastError() {
#ifdef _WIN32
            string ret;

            wchar_t* s = NULL;

            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, (DWORD)GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPWSTR)&s, 0, NULL);
            ret = su::format("(error_code={}) {}", GetLastError(), su::toString(wstring(s)));
            LocalFree(s);

            return ret;
#elif __linux__
            return su::format("errno={} ({})", errno, strerror(errno));
#endif
        }
    };
}