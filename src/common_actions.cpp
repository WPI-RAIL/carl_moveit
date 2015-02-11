#include <carl_moveit/common_actions.h>

using namespace std;

CommonActions::CommonActions() :
    moveToJointPoseClient("carl_moveit_wrapper/move_to_joint_pose"),
    readyArmServer(n, "carl_moveit_wrapper/common_actions/ready_arm", boost::bind(&CommonActions::readyArm, this, _1), false)
{
  //setup home position
  homePosition.resize(NUM_JACO_JOINTS);
  homePosition[0] = -1.410;
  homePosition[1] = 2.975;
  homePosition[2] = .868;
  homePosition[3] = -2.323;
  homePosition[4] = 1.626;
  homePosition[5] = 1.393;

  angularCmdPublisher = n.advertise<wpi_jaco_msgs::AngularCommand>("jaco_arm/angular_cmd", 1);

  //start action server
  readyArmServer.start();
}

void CommonActions::readyArm(const wpi_jaco_msgs::HomeArmGoalConstPtr &goal)
{
  ROS_INFO("Ready callback");
  carl_moveit::MoveToJointPoseGoal jointPoseGoal;
  wpi_jaco_msgs::HomeArmResult result;

  vector<float> baseJointPoseGoal;
  baseJointPoseGoal.resize(homePosition.size());
  for (unsigned int i = 0; i < baseJointPoseGoal.size(); i ++)
  {
    baseJointPoseGoal[i] = homePosition[i];
  }
  jointPoseGoal.joints.resize(baseJointPoseGoal.size());
  for (unsigned int i = 0; i < jointPoseGoal.joints.size(); i ++)
  {
    jointPoseGoal.joints[i] = baseJointPoseGoal[i];
  }
  bool succeeded = false;
  int counter = 0;
  int attempts = max((int)(goal->numAttempts), 1);
  while (!succeeded && counter < attempts)
  {
    ROS_INFO("Ready arm attempt %d", counter);

    moveToJointPoseClient.sendGoal(jointPoseGoal);
    ROS_INFO("Moving arm to ready position...");
    while (!moveToJointPoseClient.getState().isDone())
    {
      if (readyArmServer.isPreemptRequested() || !ros::ok())
      {
        ROS_INFO("Ready arm action preempted.");
        moveToJointPoseClient.cancelAllGoals();
        result.success = false;
        readyArmServer.setPreempted(result);
        return;
      }
    }

    carl_moveit::MoveToJointPoseResultConstPtr readyResult = moveToJointPoseClient.getResult();

    succeeded = readyResult->success;
    counter ++;

    //slightly vary joint goal and retry planning
    if (!succeeded && counter < attempts)
    {
      ROS_INFO("Ready arm failed, resampling goal for another attempt...");
      for (unsigned int i = 0; i < jointPoseGoal.joints.size(); i ++)
      {
        jointPoseGoal.joints[i] = baseJointPoseGoal[i] + (rand() % 700 - 350) / 10000;  //vary by up to ~2 degrees
      }
    }
  }

  if (!succeeded)
  {
    ROS_INFO("Plan and move to ready position failed.");
    result.success = false;
    readyArmServer.setSucceeded(result);
  }

  if (goal->retract)
  {
    angularCmdPublisher.publish(goal->retractPosition);
    ros::Duration(3.0).sleep(); //wait for retract to finish, the time is pretty consistent...
  }

  ROS_INFO("Ready arm finished.");

  result.success = succeeded;
  readyArmServer.setSucceeded(result);
}

int main(int argc, char **argv)
{
  ros::init(argc, argv, "carl_moveit_common_actions");

  CommonActions ca;

  ros::spin();
}