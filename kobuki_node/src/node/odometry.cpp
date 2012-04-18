/**
 * @file /kobuki_node/src/node/odometry.cpp
 *
 * @brief File comment
 *
 * File comment
 *
 * @date 15/04/2012
 **/

/*****************************************************************************
** Includes
*****************************************************************************/

#include "../../include/kobuki_node/odometry.hpp"

/*****************************************************************************
** Namespaces
*****************************************************************************/

namespace kobuki {

/*****************************************************************************
** Implementation
*****************************************************************************/

Odometry::Odometry () :
  odom_frame("odom"),
  base_frame("base_footprint"),
  publish_tf(false)
{};

void Odometry::init(ros::NodeHandle& nh, const std::string& name) {
  double timeout;
  nh.param("cmd_vel_timeout", timeout, 0.6);
  cmd_vel_timeout.fromSec(timeout);
  ROS_INFO_STREAM("Kobuki : Velocity commands timeout: " << cmd_vel_timeout << " seconds [" << name << "].");

  if (!nh.getParam("odom_frame", odom_frame)) {
    ROS_WARN_STREAM("Kobuki : no param server setting for odom_frame, using default [" << odom_frame << "][" << name << "].");
  } else {
    ROS_INFO_STREAM("Kobuki : using odom_frame [" << odom_frame << "][" << name << "].");
  }

  if (!nh.getParam("base_frame", base_frame)) {
    ROS_WARN_STREAM("Kobuki : no param server setting for base_frame, using default [" << base_frame << "][" << name << "].");
  } else {
    ROS_INFO_STREAM("Kobuki : using base_frame [" << base_frame << "][" << name << "].");
  }

  if (!nh.getParam("publish_tf", publish_tf)) {
    ROS_WARN_STREAM("Kobuki : no param server setting for publish_tf, using default [" << publish_tf << "][" << name << "].");
  } else {
    if ( publish_tf ) {
      ROS_INFO_STREAM("Kobuki : publishing transforms [" << name << "].");
    } else {
      ROS_INFO_STREAM("Kobuki : not publishing transforms (see robot_pose_ekf) [" << name << "].");
    }
  }

  odom_trans.header.frame_id = odom_frame;
  odom_trans.child_frame_id = base_frame;
  odom.header.frame_id = odom_frame;
  odom.child_frame_id = base_frame;

  // Pose covariance (required by robot_pose_ekf) TODO: publish realistic values
  // Odometry yaw covariance must be much bigger than the covariance provided
  // by the imu, as the later takes much better measures
  odom.pose.covariance[0]  = 0.1;
  odom.pose.covariance[7]  = 0.1;
  odom.pose.covariance[35] = 0.2;

  odom.pose.covariance[14] = DBL_MAX; // set a very large covariance on unused
  odom.pose.covariance[21] = DBL_MAX; // dimensions (z, pitch and roll); this
  odom.pose.covariance[28] = DBL_MAX; // is a requirement of robot_pose_ekf

  pose.setIdentity();

  odom_publisher = nh.advertise<nav_msgs::Odometry>("odom", 50); // topic name and queue size
}

bool Odometry::commandTimeout() const {
  if ( (!last_cmd_time.isZero()) && ((ros::Time::now() - last_cmd_time) > cmd_vel_timeout) ) {
    return true;
  } else {
    return false;
  }
}

void Odometry::update(const ecl::Pose2D<double> &pose_update, ecl::linear_algebra::Vector3d &pose_update_rates) {
  pose *= pose_update;

  //since all ros tf odometry is 6DOF we'll need a quaternion created from yaw
  geometry_msgs::Quaternion odom_quat = tf::createQuaternionMsgFromYaw(pose.heading());

  if ( ros::ok() ) {
    publishTransform(odom_quat);
    publishOdometry(odom_quat, pose_update_rates);
  }
}

/*****************************************************************************
** Private Implementation
*****************************************************************************/

void Odometry::publishTransform(const geometry_msgs::Quaternion &odom_quat)
{
  if (publish_tf == false)
    return;

  odom_trans.header.stamp = ros::Time::now();
  odom_trans.transform.translation.x = pose.x();
  odom_trans.transform.translation.y = pose.y();
  odom_trans.transform.translation.z = 0.0;
  odom_trans.transform.rotation = odom_quat;
  odom_broadcaster.sendTransform(odom_trans);
}

void Odometry::publishOdometry(const geometry_msgs::Quaternion &odom_quat,
                             const ecl::linear_algebra::Vector3d &pose_update_rates)
{
  odom.header.stamp = ros::Time::now();

  // Position
  odom.pose.pose.position.x = pose.x();
  odom.pose.pose.position.y = pose.y();
  odom.pose.pose.position.z = 0.0;
  odom.pose.pose.orientation = odom_quat;

  // Velocity
  odom.twist.twist.linear.x = pose_update_rates[0];
  odom.twist.twist.linear.y = pose_update_rates[1];
  odom.twist.twist.angular.z = pose_update_rates[2];

  odom_publisher.publish(odom);
}

} // namespace kobuki