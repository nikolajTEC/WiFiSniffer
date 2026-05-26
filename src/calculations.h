// calculations.h
#ifndef CALCULATIONS_H
#define CALCULATIONS_H

#include <cmath>
#include "secrets.h"

// ═══════════════════════════════════════════════════════════════
//  DISTANCE ESTIMATION
// ═══════════════════════════════════════════════════════════════
const int   RSSI_REF  = -59;
const float PATH_LOSS = 2.3f;

float rssiToMeters(int8_t rssi) {
    return powf(10.0f, (float)(RSSI_REF - rssi) / (10.0f * PATH_LOSS));
}

// ═══════════════════════════════════════════════════════════════
//  TRILATERATION
// ═══════════════════════════════════════════════════════════════
bool trilaterate(float r0, float r1, float r2,
                 float& outX, float& outY) {

    float x0 = NODE_POS[0][0];
    float y0 = NODE_POS[0][1];

    float x1 = NODE_POS[1][0];
    float y1 = NODE_POS[1][1];

    float x2 = NODE_POS[2][0];
    float y2 = NODE_POS[2][1];

    float A = 2.0f * (x1 - x0);
    float B = 2.0f * (y1 - y0);
    float C = r0*r0 - r1*r1 + x1*x1 - x0*x0 + y1*y1 - y0*y0;

    float D = 2.0f * (x2 - x0);
    float E = 2.0f * (y2 - y0);
    float F = r0*r0 - r2*r2 + x2*x2 - x0*x0 + y2*y2 - y0*y0;

    float det = A * E - B * D;

    if (fabsf(det) < 0.0001f)
        return false;

    outX = (C * E - B * F) / det;
    outY = (A * F - C * D) / det;

    return true;
}

#endif // CALCULATIONS_H
