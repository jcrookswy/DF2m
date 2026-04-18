#pragma once

// CCompass — 3-axis magnetoresistive compass with tilt compensation and
//            magnetic declination correction.
//
// Device-frame axis convention (same as NED):
//   X = forward / bow       (+ toward nose)
//   Y = right / starboard   (+ toward right side)
//   Z = down                (+ toward ground)
//
// Bearing convention: 0° = North, 90° = East, increases clockwise, range [0, 360).

class CCompass
{
public:
    // Constructor.
    //
    // lat, lon     : WGS-84 decimal degrees. Used to estimate magnetic declination
    //                via a dipole model (accuracy ±5–15°). For precise results, call
    //                SetDeclination() afterward with the value from:
    //                  https://www.ngdc.noaa.gov/geomag/calculators/magcalc.shtml
    //
    // mountYaw     : Angle (degrees, CW from device forward) of the chip's +X axis
    //                relative to the device's +X axis (e.g. 90° if chip is turned
    //                sideways so its X points toward the device's right side).
    //
    // mountPitch   : Nose-up tilt of the chip relative to the device horizontal
    //                plane (degrees, positive = chip X-axis tilted upward).
    //
    // mountRoll    : Roll of the chip about the device forward axis (degrees,
    //                positive = right side of chip tilted downward).
    CCompass(double lat, double lon,
             double mountYaw   = 0.0,
             double mountPitch = 0.0,
             double mountRoll  = 0.0);

    // Override the auto-computed declination with a known value.
    // declinationDeg: degrees East positive, West negative.
    void SetDeclination(double declinationDeg);

    // Magnetic bearing (degrees [0,360), clockwise from magnetic North).
    //
    // x, y, z      : Raw magnetometer readings in any consistent linear units (e.g. µT).
    // devicePitch  : Current device pitch (degrees, nose-up positive). Pass 0 if level.
    // deviceRoll   : Current device roll  (degrees, right-side-down positive). Pass 0 if level.
    double GetMagneticBearing(double x, double y, double z,
                              double devicePitch = 0.0,
                              double deviceRoll  = 0.0) const;

    // True bearing (degrees [0,360)) = magnetic bearing corrected for declination.
    double GetTrueBearing(double x, double y, double z,
                          double devicePitch = 0.0,
                          double deviceRoll  = 0.0) const;

    double GetDeclination() const { return m_declination; }

private:
    double m_declination;   // degrees, East positive
    double m_R[3][3];       // pre-computed mounting rotation matrix (sensor → device frame)

    // Rough declination estimate from lat/lon using the WMM 2025 dipole model.
    static double ComputeDeclination(double latDeg, double lonDeg);

    // Wrap angle to [0, 360).
    static double NormalizeBearing(double deg);
};
