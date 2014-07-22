/*
 * Copyright © 2012 Intel Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#include <HwcTrace.h>
#include <tangier/TngPlaneManager.h>
#include <tangier/TngPrimaryPlane.h>
#include <tangier/TngSpritePlane.h>
#include <tangier/TngOverlayPlane.h>

namespace android {
namespace intel {

TngPlaneManager::TngPlaneManager()
    : DisplayPlaneManager()
{
    memset(&mZorder, 0, sizeof(mZorder));
}

TngPlaneManager::~TngPlaneManager()
{
}

bool TngPlaneManager::initialize()
{
    mSpritePlaneCount = 1;  // Sprite D
    mOverlayPlaneCount = 2;  // Overlay A & C
    mPrimaryPlaneCount = 3;  // Primary A, B, C

    return DisplayPlaneManager::initialize();
}

void TngPlaneManager::deinitialize()
{
    DisplayPlaneManager::deinitialize();
}

DisplayPlane* TngPlaneManager::allocPlane(int index, int type)
{
    DisplayPlane *plane = 0;

    switch (type) {
    case DisplayPlane::PLANE_PRIMARY:
        plane = new TngPrimaryPlane(index, index);
        break;
    case DisplayPlane::PLANE_SPRITE:
        plane = new TngSpritePlane(index, 0);
        break;
    case DisplayPlane::PLANE_OVERLAY:
        plane = new TngOverlayPlane(index, 0);
        break;
    default:
        ETRACE("unsupported type %d", type);
        break;
    }
    if (plane && !plane->initialize(DisplayPlane::MIN_DATA_BUFFER_COUNT)) {
        ETRACE("failed to initialize plane.");
        DEINIT_AND_DELETE_OBJ(plane);
    }

    return plane;
}

bool TngPlaneManager::isValidZOrder(int dsp, ZOrderConfig& config)
{
    // check whether it's a supported z order config
    int firstRGB = -1;
    int lastRGB = -1;
    int firstOverlay = -1;
    int lastOverlay = -1;

    for (int i = 0; i < (int)config.size(); i++) {
        const ZOrderLayer *layer = config[i];
        switch (layer->planeType) {
        case DisplayPlane::PLANE_PRIMARY:
        case DisplayPlane::PLANE_SPRITE:
            if (firstRGB == -1) {
                firstRGB = i;
                lastRGB = i;
            } else {
                lastRGB = i;
            }
            break;
        case DisplayPlane::PLANE_OVERLAY:
            if (firstOverlay == -1) {
                firstOverlay = i;
                lastOverlay = i;
            } else {
                lastOverlay = i;
            }
            break;
        }
    }

    if ((lastRGB < firstOverlay) || (firstRGB > lastOverlay)) {
        return true;
    } else {
        VTRACE("invalid z order config. rgb (%d, %d) yuv (%d, %d)",
               firstRGB, lastRGB, firstOverlay, lastOverlay);
        return false;
    }
}

bool TngPlaneManager::assignPlanes(int dsp, ZOrderConfig& config)
{
    // probe if plane is available
    int size = (int)config.size();
    for (int i = 0; i < size; i++) {
        const ZOrderLayer *layer = config.itemAt(i);
        if (!getFreePlanes(dsp, layer->planeType)) {
            DTRACE("no plane available for dsp %d, type %d", dsp, layer->planeType);
            return false;
        }
    }

    if (config.size() == 1 && config[0]->planeType == DisplayPlane::PLANE_SPRITE) {
        config[0]->planeType == DisplayPlane::PLANE_PRIMARY;
    }

    // allocate planes
    for (int i = 0; i < size; i++) {
        ZOrderLayer *layer = config.itemAt(i);
        layer->plane = getPlaneHelper(dsp, layer->planeType);
        if (layer->plane == NULL) {
            // should never happen!!
            ETRACE("failed to assign plane for type %d", layer->planeType);
        }
        // sequence !!!!! enabling plane before setting zorder
        // see TngSpritePlane::enablePlane implementation!!!!
        layer->plane->enable();
    }

    // setup Z order
    for (int i = 0; i < size; i++) {
        ZOrderLayer *layer = config.itemAt(i);
        layer->plane->setZOrderConfig(config, &mZorder);
    }

    return true;
}

void* TngPlaneManager::getZOrderConfig() const
{
    return (void*)&mZorder;
}

DisplayPlane* TngPlaneManager::getPlaneHelper(int dsp, int type)
{
    RETURN_NULL_IF_NOT_INIT();

    if (dsp < 0 || dsp > IDisplayDevice::DEVICE_EXTERNAL) {
        ETRACE("Invalid display device %d", dsp);
        return 0;
    }

    int index = dsp == IDisplayDevice::DEVICE_PRIMARY ? 0 : 1;

    if (type == DisplayPlane::PLANE_PRIMARY) {
        return getPlane(type, index);
    } else if (type == DisplayPlane::PLANE_SPRITE) {
        return getAnyPlane(type);
    } else if (type == DisplayPlane::PLANE_OVERLAY) {
        // use overlay A for pipe A and overlay C for pipe B if possible
        DisplayPlane *plane = getPlane(type, index);
        if (plane == NULL) {
            plane = getPlane(type, !index);
        }
        return plane;
    } else {
        ETRACE("invalid plane type %d", type);
        return 0;
    }
}

} // namespace intel
} // namespace android
