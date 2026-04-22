///////////////////////////////////////////////////////////////////////////////
// AOA DF 2m — Angle-of-Arrival Direction Finder for 2m Amateur Radio Band
// File:    CCompass.cpp
// Author:  Justin Crooks
// Copyright (C) 2025  Justin Crooks
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
///////////////////////////////////////////////////////////////////////////////
#define _USE_MATH_DEFINES
#include <cmath>
#include "CCompass.h"

// ─── internal helpers ────────────────────────────────────────────────────────

static inline double toRad(double deg) { return deg * M_PI / 180.0; }
static inline double toDeg(double rad) { return rad * 180.0 / M_PI; }

double CCompass::NormalizeBearing(double deg)
{
    deg = fmod(deg, 360.0);
    if (deg < 0.0) deg += 360.0;
    return deg;
}

// ─── constructor ─────────────────────────────────────────────────────────────

CCompass::CCompass(double lat, double lon,
                   double mountYaw, double mountPitch, double mountRoll)
{
    m_declination = ComputeDeclination(lat, lon);

    // Pre-compute rotation matrix R that transforms a field vector from sensor
    // frame into device frame.  Uses ZYX Euler angles applied in order:
    // yaw (rotation about Z), then pitch (about Y), then roll (about X).
    //
    // R = Rz(yaw) * Ry(pitch) * Rx(roll)
    double cy = cos(toRad(mountYaw)),   sy = sin(toRad(mountYaw));
    double cp = cos(toRad(mountPitch)), sp = sin(toRad(mountPitch));
    double cr = cos(toRad(mountRoll)),  sr = sin(toRad(mountRoll));

    m_R[0][0] =  cy * cp;
    m_R[0][1] =  cy * sp * sr - sy * cr;
    m_R[0][2] =  cy * sp * cr + sy * sr;

    m_R[1][0] =  sy * cp;
    m_R[1][1] =  sy * sp * sr + cy * cr;
    m_R[1][2] =  sy * sp * cr - cy * sr;

    m_R[2][0] = -sp;
    m_R[2][1] =  cp * sr;
    m_R[2][2] =  cp * cr;
}

void CCompass::SetDeclination(double declinationDeg)
{
    m_declination = declinationDeg;
}

// ─── bearing computation ──────────────────────────────────────────────────────

double CCompass::GetMagneticBearing(double x, double y, double z,
                                    double devicePitch, double deviceRoll) const
{
    // Step 1: rotate sensor readings into device frame via the mount matrix.
    double bx = m_R[0][0]*x + m_R[0][1]*y + m_R[0][2]*z;
    double by = m_R[1][0]*x + m_R[1][1]*y + m_R[1][2]*z;
    double bz = m_R[2][0]*x + m_R[2][1]*y + m_R[2][2]*z;

    // Step 2: tilt compensation — project device-frame field onto the horizontal
    //         plane using device pitch and roll from an accelerometer (or IMU).
    //         Formula per Freescale application note AN4248.
    //         pitch: nose-up positive;  roll: right-side-down positive.
    double cp = cos(toRad(devicePitch)), sp = sin(toRad(devicePitch));
    double cr = cos(toRad(deviceRoll)),  sr = sin(toRad(deviceRoll));

    double bhx = bx * cp + by * sp * sr + bz * sp * cr;  // horizontal North component
    double bhy = by * cr - bz * sr;                       // horizontal East component

    // Step 3: compute bearing.  atan2(-bhy, bhx) maps:
    //   (bhx > 0, bhy = 0)  → 0°   (pointing North)
    //   (bhx = 0, bhy < 0)  → 90°  (pointing East)
    //   (bhx < 0, bhy = 0)  → 180° (pointing South)
    //   (bhx = 0, bhy > 0)  → 270° (pointing West)
    return NormalizeBearing(toDeg(atan2(-bhy, bhx)));
}

double CCompass::GetTrueBearing(double x, double y, double z,
                                double devicePitch, double deviceRoll) const
{
    return NormalizeBearing(
        GetMagneticBearing(x, y, z, devicePitch, deviceRoll) + m_declination);
}

// ─── declination estimate ─────────────────────────────────────────────────────

double CCompass::ComputeDeclination(double latDeg, double lonDeg)
{
    // Dipole-only approximation using WMM 2025.0 Gauss coefficients (degree n=1).
    // Gives a rough starting value; accuracy is typically ±5–15° because higher-
    // degree non-dipole terms (which dominate declination in many regions) are
    // omitted.  Always call SetDeclination() with the true value for your location
    // before relying on true-bearing output:
    //   https://www.ngdc.noaa.gov/geomag/calculators/magcalc.shtml
    //
    // WMM 2025.0 main-field, n=1 coefficients (nT):
    const double g10 = -29351.8;
    const double g11 =  -1585.9;
    const double h11 =   4945.4;

    double lat = toRad(latDeg);
    double lon = toRad(lonDeg);

    // Surface field components from the centered-dipole potential:
    //   X (North) = -g10*cos(lat) + (g11*cos(lon) + h11*sin(lon))*sin(lat)
    //   Y (East)  =  g11*sin(lon) - h11*cos(lon)
    double X = -g10 * cos(lat) + (g11 * cos(lon) + h11 * sin(lon)) * sin(lat);
    double Y =  g11 * sin(lon) - h11 * cos(lon);

    return toDeg(atan2(Y, X));
}
