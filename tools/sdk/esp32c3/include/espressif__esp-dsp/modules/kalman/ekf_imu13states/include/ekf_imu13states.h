// Copyright 2020-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _ekf_imu13states_H_
#define _ekf_imu13states_H_

#include "ekf.h"

/**
* @brief This class is used to process and calculate attitude from imu sensors.
*
*   The class use state vector with 13 follows values
*   X[0..3] - attitude quaternion
*   X[4..6] - gyroscope bias error, rad/sec
*   X[7..9] - magnetometer vector value - magn_ampl
*   X[10..12] - magnetometer offset value - magn_offset
*
*   where, reference magnetometer value = magn_ampl*rotation_matrix' + magn_offset
*/
class ekf_imu13states: public ekf {
public:
    ekf_imu13states();
    virtual ~ekf_imu13states();
    virtual void Init();

    // Method calculates Xdot values depends on U
    // U - gyroscope values in radian per seconds (rad/sec)
    virtual dspm::Mat StateXdot(dspm::Mat &x, float *u);
    virtual void LinearizeFG(dspm::Mat &x, float *u);

    /**
    *     Method for development and tests only.
    */
    void Test();
    /**
    *     Method for development and tests only.
    * 
    * @param[in] enable_att - enable attitude as input reference value
    */
    void TestFull(bool enable_att);

    /**
    *     Initial reference valie for magnetometer.
    */
    dspm::Mat mag0;
    /**
    *     Initial reference valie for accelerometer.
    */
    dspm::Mat accel0;

    /**
    * number of control measurements
    */
    int NUMU; 

    /**
     * Update part of system state by reference measurements accelerometer and magnetometer.
     * Only attitude and gyro bias will be updated.
     * This method should be used as main method after calibration.
     * 
     * @param[in] accel_data: accelerometer measurement vector XYZ in g, where 1 g ~ 9.81 m/s^2
     * @param[in] magn_data: magnetometer measurement vector XYZ
     * @param[in] R: measurement noise covariance values for diagonal covariance matrix. Then smaller value, then more you trust them.
     */
    void UpdateRefMeasurement(float *accel_data, float *magn_data, float R[6]);
    /**
     * Update full system state by reference measurements accelerometer and magnetometer.
     * This method should be used at calibration phase.
     * 
     * @param[in] accel_data: accelerometer measurement vector XYZ in g, where 1 g ~ 9.81 m/s^2
     * @param[in] magn_data: magnetometer measurement vector XYZ
     * @param[in] R: measurement noise covariance values for diagonal covariance matrix. Then smaller value, then more you trust them.
     */
    void UpdateRefMeasurementMagn(float *accel_data, float *magn_data, float R[6]);
    /**
     * Update system state by reference measurements accelerometer, magnetometer and attitude quaternion.
     * This method could be used when system on constant state or in initialization phase.
     * @param[in] accel_data: accelerometer measurement vector XYZ in g, where 1 g ~ 9.81 m/s^2
     * @param[in] magn_data: magnetometer measurement vector XYZ
     * @param[in] attitude: attitude quaternion
     * @param[in] R: measurement noise covariance values for diagonal covariance matrix. Then smaller value, then more you trust them.
     */
    void UpdateRefMeasurement(float *accel_data, float *magn_data, float *attitude, float R[10]);

};

#endif // _ekf_imu13states_H_
