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


#ifndef _ekf_h_
#define _ekf_h_

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <mat.h>

/**
 * The ekf is a base class for Extended Kalman Filter.
 * It contains main matrix operations and define the processing flow.
 */
class ekf {
public:

    /**
     * Constructor of EKF.
     * THe constructor allocate main memory for the matrixes.
     * @param[in] x: - amount of states in EKF. x[n] = F*x[n-1] + G*u + W. Size of matrix F
     * @param[in] w: - amount of control measurements and noise inputs. Size of matrix G
    */
    ekf(int x, int w);


    /**
     * Distructor of EKF
    */
    virtual ~ekf();
    /**
     * Main processing method of the EKF.
     *
     * @param[in] u: - input measurements
     * @param[in] dt: - time difference from the last call in seconds
    */
    virtual void Process(float *u, float dt);


    /**
     * Initialization of EKF.
     * The method should be called befare the first use of the filter.
    */
    virtual void Init() = 0;
    /**
     * x[n] = F*x[n-1] + G*u + W
     * Number of states, X is the state vector (size of F matrix)
    */
    int NUMX;
    /**
     * x[n] = F*x[n-1] + G*u + W
     * The size of G matrix
    */
    int NUMW;

    /**
     * System state vector
    */
    dspm::Mat &X;

    /**
     * Linearized system matrices F, where x[n] = F*x[n-1] + G*u + W
    */
    dspm::Mat &F;
    /**
     * Linearized system matrices G, where x[n] = F*x[n-1] + G*u + W
    */
    dspm::Mat &G;

    /**
    * Covariance matrix and state vector
    */
    dspm::Mat &P;

    /**
     * Input noise and measurement noise variances
    */
    dspm::Mat &Q;

    /**
     * Runge-Kutta state update method.
     * The method calculates derivatives of input vector x and control measurements u
     *
     * @param[in] x: state vector
     * @param[in] u: control measurement
     * @param[in] dt: time interval from last update in seconds
     */
    void RungeKutta(dspm::Mat &x, float *u, float dt);

    // System Dependent methods:

    /**
     * Derivative of state vector X
     * Re
     * @param[in] x: state vector
     * @param[in] u: control measurement
     * @return
     *      - derivative of input vector x and u
     */
    virtual dspm::Mat StateXdot(dspm::Mat &x, float *u);
    /**
     * Calculation of system state matrices F and G
     * @param[in] x: state vector
     * @param[in] u: control measurement
     */
    virtual void LinearizeFG(dspm::Mat &x, float *u) = 0;
    //

    // System independent methods

    /**
     * Calculates covariance prediction matrux P.
     * Update matrix P
     * @param[in] dt: time interval from last update
     */
    virtual void CovariancePrediction(float dt);

    /**
     * Update of current state by measured values.
     * Optimized method for non correlated values
     * Calculate Kalman gain and update matrix P and vector X.
     * @param[in] H: derivative matrix
     * @param[in] measured: array of measured values
     * @param[in] expected: array of expected values
     * @param[in] R: measurement noise covariance values
     */
    virtual void Update(dspm::Mat &H, float *measured, float *expected, float *R);
    /**
     * Update of current state by measured values.
     * This method just as a reference for research purpose.
     * Not used in real calculations.
     * @param[in] H: derivative matrix
     * @param[in] measured: array of measured values
     * @param[in] expected: array of expected values
     * @param[in] R: measurement noise covariance values
     */
    virtual void UpdateRef(dspm::Mat &H, float *measured, float *expected, float *R);

    /**
     * Matrix for intermidieve calculations
    */
    float *HP;
    /**
     * Matrix for intermidieve calculations
    */
    float *Km;

public:
    // Additional universal helper methods
    /**
     * Convert quaternion to rotation matrix.
     * @param[in] q: quaternion
     *
     * @return
     *      - rotation matrix 3x3
     */
    static dspm::Mat quat2rotm(float q[4]);

    /**
     * Convert rotation matrix to quaternion.
     * @param[in] R: rotation matrix
     *
     * @return
     *      - quaternion 4x1
     */
    static dspm::Mat rotm2quat(dspm::Mat &R);

    /**
     * Convert quaternion to Euler angels.
     * @param[in] q: quaternion
     *
     * @return
     *      - Euler angels 3x1
     */
    static dspm::Mat quat2eul(const float q[4]);
    /**
     * Convert Euler angels to rotation matrix.
     * @param[in] xyz: Euler angels
     *
     * @return
     *      - rotation matrix 3x3
     */
    static dspm::Mat eul2rotm(float xyz[3]);

    /**
     * Convert rotation matrix to Euler angels.
     * @param[in] rotm: rotation matrix
     *
     * @return
     *      - Euler angels 3x1
     */
    static dspm::Mat rotm2eul(dspm::Mat &rotm);

    /**
     * Df/dq:  Derivative of vector by quaternion.
     * @param[in] vector: input vector
     * @param[in] quat: quaternion
     *
     * @return
     *      - Derivative matrix 3x4
     */
    static dspm::Mat dFdq(dspm::Mat &vector, dspm::Mat &quat);

    /**
     * Df/dq: Derivative of vector by inverted quaternion.
     * @param[in] vector: input vector
     * @param[in] quat: quaternion
     *
     * @return
     *      - Derivative matrix 3x4
     */
    static dspm::Mat dFdq_inv(dspm::Mat &vector, dspm::Mat &quat);

    /**
     * Make skew-symmetric matrix of vector.
     * @param[in] w: source vector
     *
     * @return
     *      - skew-symmetric matrix 4x4
     */
    static dspm::Mat SkewSym4x4(float *w);

    // q product
    // Rl = [q(1) - q(2) - q(3) - q(4); ...
    //      q(2)  q(1) - q(4)  q(3); ...
    //      q(3)  q(4)  q(1) - q(2); ...
    //      q(4) - q(3)  q(2)  q(1); ...

    /**
     * Make right quaternion-product matrices.
     * @param[in] q: source quaternion
     *
     * @return
     *      - right quaternion-product matrix 4x4
     */
    static dspm::Mat qProduct(float *q);

};

#endif // _ekf_h_
