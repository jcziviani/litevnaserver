// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2025 Julio Cesar Ziviani Alvarez

#pragma once

#include <complex>
#include <thread>

#include "lib/SerialPort.h"

#define LITEVNA_CLEAR_FIFO 0,0,0,0,0,0,0,0

#define LITEVNA_CMD_INDICATE  0x0D
#define LITEVNA_CMD_READ1     0x10
#define LITEVNA_CMD_READ2     0x11
#define LITEVNA_CMD_READ4     0x12
#define LITEVNA_CMD_READ_FIFO 0x18
#define LITEVNA_CMD_WRITE1    0x20
#define LITEVNA_CMD_WRITE2    0x21
#define LITEVNA_CMD_WRITE4    0x22
#define LITEVNA_CMD_WRITE8    0x23

#define LITEVNA_REG_SWEEP_START           0x00
#define LITEVNA_REG_SWEEP_STEP            0x10
#define LITEVNA_REG_SWEEP_POINTS          0x20
#define LITEVNA_REG_VALUES_PER_FREQUENCY  0x22
#define LITEVNA_REG_SAMPLES_MODE          0x26
#define LITEVNA_REG_READ_FIFO             0x30
#define LITEVNA_REG_DEVICE_VARIANT        0xF0
#define LITEVNA_REG_PROTOCOL_VERSION      0xF1

#define LITEVNA_SEND_ALL_POINTS           0x00

#define LITEVNA_SAMPLES_MODE_APP_CALIBRATION 0x01
#define LITEVNA_SAMPLES_MODE_LEAVE 0x02
#define LITEVNA_SAMPLES_MODE_DEVICE_CALIBRATION 0x03

#define LITEVNA_PI        3.141592654f
#define LITEVNA_VSWR_MAX  100000

namespace litevnaserver {
#pragma pack(push, 1)
    struct LiteVNAFifoData
    {
        int32_t  channel0OutRe;
        int32_t  channel0OutIm;
        int32_t  channel0InRe;
        int32_t  channel0InIm;
        int32_t  channel1InRe;
        int32_t  channel1InIm;
        uint16_t freqIndex;
        uint8_t  reserved[5];
        uint8_t  checksum;
    };
#pragma pack(pop)

    struct ScanValues {
        vector<complex<float>> channel0Out;
        vector<complex<float>> channel0In;
        vector<complex<float>> channel1In;
    };

    class LiteVNA {
    public:
        // 1. Lifecycle
        LiteVNA() = default;
        LiteVNA(const LiteVNA&) = delete;
        LiteVNA& operator=(const LiteVNA&) = delete;
        LiteVNA(const LiteVNA&&) = delete;
        LiteVNA& operator=(const LiteVNA&&) = delete;
        ~LiteVNA() = default;

        Result initialize() {
            Result result = serial->open(config->comPort, BaudRate::_115200, Parity::None, 8, StopBits::One);

            if (result) {
                return result;
            }
            result = clearFifo();

            if (result) {
                return result;
            }
            result = leaveDataMode();

            if (result) {
                return result;
            }
            result = checkIndicate();

            if (result) {
                return result;
            }
            result = checkDeviceVariant();

            if (result) {
                return result;
            }
            result = checkProtocolVersion();

            LOGGER(Info, "Found LiteVNA at com port {}", config->comPort);
            return result;
        }

        void terminate() {
            serial->close();
        }

        // 2. Dependency injection
        void setConfig(Config* _config) {
            config = _config;
        }

        // 3. Functionalities
        Result scan(uint64_t start, uint64_t step, uint16_t points, ScanValues& values) {
            LOGGER(LiteVNA, "Scanning start={}, step={}, points={}", start, step, points);

            Result result = clearFifo();

            if (result) {
                return result;
            }
            result = enterDataMode();

            if (result) {
                return result;
            }
            result = sendCmdWrite8("Sending `Sweep start value`", LITEVNA_REG_SWEEP_START, start);

            if (result) {
                return result;
            }
            result = sendCmdWrite8("Sending `Sweep step value`", LITEVNA_REG_SWEEP_STEP, step);

            if (result) {
                return result;
            }
            result = sendCmdWrite2("Sending `Sweep points value`", LITEVNA_REG_SWEEP_POINTS, points);

            if (result) {
                return result;
            }
            result = sendCmdWrite2("Sending `Values per frequency`", LITEVNA_REG_VALUES_PER_FREQUENCY, 1);

            if (result) {
                return result;
            }
            this_thread::sleep_for(chrono::milliseconds(50));

            result = readFifo();

            if (result) {
                return result;
            }
            values.channel0Out.resize(points);
            values.channel0In.resize(points);
            values.channel1In.resize(points);

            size_t count = 0;

            while (count < points) {
                uint8_t buffer[sizeof(LiteVNAFifoData)];
                size_t bufferPos = 0;
                size_t totalRead = 0;
                uint64_t start = DateTime::nowMilliseconds();

                while (bufferPos < sizeof(LiteVNAFifoData)) {
                    result = serial->read(buffer + bufferPos, sizeof(LiteVNAFifoData) - bufferPos, &totalRead);

                    if (result) {
                        return result;
                    }
                    if (totalRead > 0) {
                        LOGGER(LiteVNA, "Received{}", formatBytes(buffer + bufferPos, totalRead));

                        bufferPos += totalRead;
                    }
                    if ((start + 10000) < DateTime::nowMilliseconds()) {
                        return Result("lite_vna_error", "Timeout reading LiteVNA Fifo data");
                    }
                }
                uint8_t checksum = 0x46;
                uint8_t* data = buffer;
                LiteVNAFifoData* fifo = (LiteVNAFifoData*)buffer;

                for (size_t i = 0; i < (sizeof(LiteVNAFifoData) - 1); i++) {
                    checksum = (checksum ^ ((checksum << 1) | 1u)) ^ data[i];
                }

                if (checksum != fifo->checksum) {
                    return Result("lite_vna_error", "Invalid Checksum `{}`", checksum);
                }
                complex<float> out0((float)fifo->channel0OutRe, (float)fifo->channel0OutIm);
                complex<float> in0 = complex<float>((float)fifo->channel0InRe, (float)fifo->channel0InIm) / out0;
                complex<float> in1 = complex<float>((float)fifo->channel1InRe, (float)fifo->channel1InIm) / out0;

                if (fifo->freqIndex >= points) {
                    return Result("lite_vna_error", "Invalid Frequency Index `{}`", fifo->freqIndex);
                }
                values.channel0Out[fifo->freqIndex] = out0;
                values.channel0In[fifo->freqIndex] = in0;
                values.channel1In[fifo->freqIndex] = in1;

                count++;
            }

            result = clearFifo();

            if (result) {
                return result;
            }
            result = leaveDataMode();

            if (result) {
                return result;
            }
            return Result::ok();
        }

        static float linear(complex<float> value) {
            return sqrtf(sumSquare(value));
        }

        static float logMag(complex<float> value) {
            const float l = sumSquare(value);
            return l == 0 ? 0.0f : log10f(l) * 10.0f;
        }

        static float phase(complex<float> value) {
            float re = value.real();
            float im = value.imag();
            return (180.0f / LITEVNA_PI) * atan2f(im, re);
        }

        static float swr(complex<float> value) {
            float x = linear(value);

            if (x > ((LITEVNA_VSWR_MAX - 1.0f) / (LITEVNA_VSWR_MAX + 1.0f))) {
                return LITEVNA_VSWR_MAX;
            }
            return (1.0f + x) / (1.0f - x);
        }

    private:
        Config* config = nullptr;
        unique_ptr<SerialPort> serial = make_unique<SerialPort>();

        Result clearFifo() {
            uint8_t buffer[] = { LITEVNA_CLEAR_FIFO };

            return write("Sending `Clear Fifo`", buffer, sizeof(buffer));
        }

        Result enterDataMode() {
            uint8_t buffer[] = { LITEVNA_CMD_WRITE1, LITEVNA_REG_SAMPLES_MODE, LITEVNA_SAMPLES_MODE_DEVICE_CALIBRATION };

            return write("Sending `Enter data mode (device calibration)`", buffer, sizeof(buffer));
        }

        Result leaveDataMode() {
            uint8_t buffer[] = { LITEVNA_CMD_WRITE1, LITEVNA_REG_SAMPLES_MODE, LITEVNA_SAMPLES_MODE_LEAVE };

            return write("Sending `Leave data mode`", buffer, sizeof(buffer));
        }

        Result readFifo() {
            uint8_t buffer[] = { LITEVNA_CMD_READ_FIFO, LITEVNA_REG_READ_FIFO, LITEVNA_SEND_ALL_POINTS };

            return write("Sending `Read Fifo`", buffer, sizeof(buffer));
        }

        Result checkIndicate() {
            uint8_t bufferReq[] = { LITEVNA_CMD_INDICATE };

            Result result = write("Sending `Indicate`", bufferReq, sizeof(bufferReq));

            if (result) {
                return result;
            }
            this_thread::sleep_for(chrono::milliseconds(50));

            uint8_t bufferResp[1];

            result = serial->read(bufferResp, sizeof(bufferResp), nullptr);

            if (result) {
                return result;
            }
            LOGGER(LiteVNA, "Received{}", formatBytes(bufferResp, sizeof(bufferResp)));

            if (bufferResp[0] != 0x32) {
                return Result("lite_vna_error", "Invalid device indicate, expected 0x32 but found 0x{}. Is LiteVNA connected to the correct com port?", su::toHex((int)bufferResp[0]));
            }
            return Result::ok();
        }

        Result checkDeviceVariant() {
            uint8_t bufferReq[] = { LITEVNA_CMD_READ1, LITEVNA_REG_DEVICE_VARIANT };

            LOGGER(LiteVNA, "Sending `Device Variant`{}", formatBytes(bufferReq, sizeof(bufferReq)));

            Result result = serial->write(bufferReq, sizeof(bufferReq));

            if (result) {
                return result;
            }
            this_thread::sleep_for(chrono::milliseconds(50));

            uint8_t bufferResp[1];

            result = serial->read(bufferResp, sizeof(bufferResp), nullptr);

            if (result) {
                return result;
            }
            LOGGER(LiteVNA, "Received{}", formatBytes(bufferResp, sizeof(bufferResp)));

            if (bufferResp[0] != 0x02) {
                return Result("lite_vna_error", "Invalid device variant, expected 0x02 but found 0x{}. Is LiteVNA connected to the correct com port?", su::toHex((int)bufferResp[0]));
            }
            return Result::ok();
        }

        Result checkProtocolVersion() {
            uint8_t bufferReq[] = { LITEVNA_CMD_READ1, LITEVNA_REG_PROTOCOL_VERSION };

            LOGGER(LiteVNA, "Sending `Protocol Version`{}", formatBytes(bufferReq, sizeof(bufferReq)));

            Result result = serial->write(bufferReq, sizeof(bufferReq));

            if (result) {
                return result;
            }
            this_thread::sleep_for(chrono::milliseconds(50));

            uint8_t bufferResp[1];

            result = serial->read(bufferResp, sizeof(bufferResp), nullptr);

            if (result) {
                return result;
            }
            LOGGER(LiteVNA, "Received{}", formatBytes(bufferResp, sizeof(bufferResp)));

            if (bufferResp[0] != 0x01) {
                return Result("lite_vna_error", "Invalidprotocol version, expected 0x01 but found 0x{}.", su::toHex((int)bufferResp[0]));
            }
            return Result::ok();
        }

        Result sendCmdWrite2(const string& text, uint8_t cmd, uint16_t value) {
            uint8_t buffer[2 + sizeof(value)];

            *buffer = LITEVNA_CMD_WRITE2;
            *(buffer + 1) = cmd;
            *((uint16_t*)(buffer + 2)) = value;

            return write(text, buffer, sizeof(buffer));
        }

        Result sendCmdWrite8(const string& text, uint8_t cmd, uint64_t value) {
            uint8_t buffer[2 + sizeof(value)];

            *buffer = LITEVNA_CMD_WRITE8;
            *(buffer + 1) = cmd;
            *((uint64_t*)(buffer + 2)) = value;

            return write(text, buffer, sizeof(buffer));
        }

        Result write(const string& text, uint8_t* buffer, size_t size) {
            LOGGER(LiteVNA, text + formatBytes(buffer, size));

            return serial->write(buffer, size);
        }

        string formatBytes(uint8_t* buffer, size_t size) {
            string hex;

            for (size_t n = 0; n < size; n++) {
                hex += " " + su::toHex(buffer[n], 2);
            }
            return hex;
        }

        static float sumSquare(complex<float> value) {
            return value.real() * value.real() + value.imag() * value.imag();
        }
    };
}