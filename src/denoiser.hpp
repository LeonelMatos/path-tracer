/**
 * @file denoiser.hpp
 * @author Leonel Matos
 * @brief OIDN denoiser
 * @date 2026-06-17
 * @copyright Copyright (c) 2026
 */

#pragma once
#include <OpenImageDenoise/oidn.hpp>
#include <vector>
#include <cstdio>

struct Denoiser {
    oidn::DeviceRef device;
    oidn::FilterRef filter;
    bool initialized = false;
    int width = 0, height = 0;

    std::vector<float> input_buf;
    std::vector<float> output_buf;

    void init(int w, int h) {
        width = w;
        height = h;

        device = oidn::newDevice();
        device.commit();

        input_buf.resize(w * h * 3);
        output_buf.resize(w * h * 3);

        filter = device.newFilter("RT");
        filter.setImage("color", input_buf.data(), oidn::Format::Float3, w, h);
        filter.setImage("output", output_buf.data(), oidn::Format::Float3, w, h);
        filter.set("hdr", true);
        filter.commit();

        initialized = true;
        printf("\n[DENOISER] Initialized %dx%d", w, h);
    }

    void resize(int w, int h) {
        initialized = false;
        init(w, h);
    }

    bool run() {
        filter.execute();
        const char* error;
        if(device.getError(error) != oidn::Error::None) {
            printf("\n[DENOISER] Error: %s\n", error);
            return false;
        }
        return true;
    }

    void cleanup() {
        filter = oidn::FilterRef();
        device = oidn::DeviceRef();
        initialized = false;
        input_buf.clear();
        output_buf.clear();
    }
};