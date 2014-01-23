/*
 *  ContactPointEstimation.cpp
 *
 *
 *  Created on: Jan 14, 2014
 *  Authors:   Francisco Viña
 *            fevb <at> kth.se
 */

/* Copyright (c) 2014, Francisco Viña, CVAP, KTH
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
      * Neither the name of KTH nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL KTH BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include <contact_point_estimation/ContactPointEstimation.h>
#include <eigen_utils/eigen_utils.h>
#include <eigen_conversions/eigen_msg.h>
#include <tf/transform_datatypes.h>


ContactPointEstimation::ContactPointEstimation(ContactPointEstimationParams *params)
{
	m_params = params;

	m_contact_point_estimate = -m_params->getInitialR();
	m_r_dot = Vector3d::Zero();
	m_Lr = Matrix3d::Zero();
	m_cr = Vector3d::Zero();
	m_surface_normal_estimate = m_params->getInitialN();
	m_Ln = Matrix3d::Zero();
	m_init = false;

}

ContactPointEstimation::~ContactPointEstimation()
{
}

void ContactPointEstimation::updateContactPointEstimate(const WrenchStamped &ft_compensated)
{
	double gamma_r = m_params->getGammaR();
	double kappa_r = m_params->getKappaR();
	double control_frequency = m_params->getControlFrequency();

	Vector3d force(ft_compensated.wrench.force.x,
			ft_compensated.wrench.force.y,
			ft_compensated.wrench.force.z);

	Vector3d torque(ft_compensated.wrench.torque.x,
			ft_compensated.wrench.torque.y,
			ft_compensated.wrench.torque.z);

	UpdateLr(force);

	Updatecr(force, torque);

	// running average to filter out noise in the update of contact point estimate
	m_r_dot = (1/20.0)*(19.0*m_r_dot - 1.0*gamma_r*(m_Lr*m_contact_point_estimate + m_cr));

	m_contact_point_estimate = m_contact_point_estimate
			+ m_r_dot *(1/control_frequency)
			- kappa_r*m_contact_point_estimate*(1/control_frequency);
}

void ContactPointEstimation::updateSurfaceNormalEstimate(const TwistStamped &twist_ft_sensor)
{
	double gamma_n = m_params->getGammaN();
	double control_frequency = m_params->getControlFrequency();
	Matrix3d Pbar_n = eigen_utils::orthProjMatrix(m_surface_normal_estimate);

	Vector3d vel(twist_ft_sensor.twist.linear.x, 
		     twist_ft_sensor.twist.linear.y, 
		     twist_ft_sensor.twist.linear.z);

	UpdateLn(vel);

	m_surface_normal_estimate = m_surface_normal_estimate - gamma_n*Pbar_n*m_Ln*m_surface_normal_estimate*(1/control_frequency);
	m_surface_normal_estimate.normalize();
}


void ContactPointEstimation::reset()
{
	m_contact_point_estimate = -m_params->getInitialR();
	m_r_dot = Vector3d::Zero();
	m_Lr = Matrix3d::Zero();
	m_cr = Vector3d::Zero();
	m_surface_normal_estimate = m_params->getInitialN();
	m_Ln = Matrix3d::Zero();
	m_init = false;
}

PointStamped ContactPointEstimation::getContactPointEstimate()
{
	PointStamped contact_point;
	contact_point.header.frame_id = m_params->getRobotFtFrameID();
	contact_point.header.stamp = ros::Time::now();
	contact_point.point.x = -m_contact_point_estimate(0);
	contact_point.point.y = -m_contact_point_estimate(1);
	contact_point.point.z = -m_contact_point_estimate(2);

	return contact_point;
}

Vector3Stamped ContactPointEstimation::getSurfaceNormalEstimate()
{
	Vector3Stamped surface_normal;
	surface_normal.header.frame_id = m_params->getRobotBaseFrameID();
	surface_normal.header.stamp = ros::Time::now();
	surface_normal.vector.x = m_surface_normal_estimate(0);
	surface_normal.vector.y = m_surface_normal_estimate(1);
	surface_normal.vector.z = m_surface_normal_estimate(2);

	return surface_normal;
}

void ContactPointEstimation::updateLr(const Vector3d& force)
{
	double beta_r = m_params->getBetaR();
	double control_frequency = m_params->getControlFrequency();

	Matrix3d Sf = eigen_utils::skewSymmetric(force);

	m_Lr = m_Lr + (-beta_r*m_Lr - Sf*Sf)*(1/control_frequency);
	m_Lr = 0.5*(m_Lr + m_Lr.transpose()); // to keep it symmetric
}

void ContactPointEstimation::updatecr(const Vector3d &force, const Vector3d& torque)
{
	double beta_r = m_params->getBetaR();
	double control_frequency = m_params->getControlFrequency();

	Matrix3d Sf = eigen_utils::skewSymmetric(force);

	m_cr = m_cr + (-beta_r*m_cr + Sf*torque)*(1/control_frequency);
}

void ContactPointEstimation::updateLn(const Vector3d &v_ft)
{
	double beta_n = m_params->getBetaN();
	double control_frequency = m_params->getControlFrequency();

	m_Ln = m_Ln + (-beta_n*m_Ln + (v_ft)*((v_ft).transpose()))*(1/control_frequency);
}
