from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction, IncludeLaunchDescription, RegisterEventHandler
from launch.substitutions import Command, FindExecutable, LaunchConfiguration, PathJoinSubstitution, PythonExpression
from launch_ros.actions import Node, PushRosNamespace
from launch_ros.substitutions import FindPackageShare
import os
from ament_index_python.packages import get_package_share_directory
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.event_handlers import OnProcessExit
import xacro
from launch.conditions import IfCondition, UnlessCondition


def generate_launch_description():

    arm_description_path = os.path.join(get_package_share_directory('armando_description'))
    
    xacro_path = os.path.join(arm_description_path, "urdf", "arm.urdf.xacro")
    
    rviz_config = os.path.join(arm_description_path, "config", "rviz", "standing.rviz")
    
    robot_description_arm_xacro =  {"robot_description": Command(['xacro ', xacro_path])}
    
    
    use_sim_arg = DeclareLaunchArgument(
        'use_sim',
        default_value='false',
        description='Use simulation time'
    )
    
    use_sim = LaunchConfiguration('use_sim')
    
    nodes_to_start = []

    robot_state_publisher_node = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        output="both",
        parameters=[robot_description_arm_xacro,
                    {"use_sim_time": use_sim}
            ]
    )

    joint_state_publisher_node = Node(
        package="joint_state_publisher",
        executable="joint_state_publisher",
        parameters=[robot_description_arm_xacro,
             {"use_sim_time": use_sim}
            ]
    )

    rviz_node = Node(
        package="rviz2",
        executable="rviz2",
        name="rviz2",
        output="log",
        arguments=["-d", rviz_config],
    )

    nodes_to_start = [
        robot_state_publisher_node,
        joint_state_publisher_node,  
        rviz_node
    ]

    return LaunchDescription([use_sim_arg]+nodes_to_start) 
