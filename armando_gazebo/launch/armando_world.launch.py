from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, RegisterEventHandler
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution, EqualsSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare
from launch.event_handlers import OnProcessExit
from launch.conditions import IfCondition

def generate_launch_description():
    
    # Launch arguments
    gui_arg = DeclareLaunchArgument(
        name='gui',
        default_value='true',
        description='Whether to launch Gazebo GUI'
    )
    urdf_package_arg = DeclareLaunchArgument(
        name='urdf_package',
        default_value='armando_description',
        description='The package where the robot description is located'
    )
    urdf_path_arg = DeclareLaunchArgument(
        name='urdf_package_path',
        default_value='urdf/arm.urdf.xacro',
        description='The path to the robot description relative to the package root'
    )
    
    # This must be the same argument from arm_controller.launch.py
    controller_type_arg = DeclareLaunchArgument(
        'controller_type',
        default_value='position',
        description='Type of controller to spawn: position or trajectory'
    )
    
    # Include empty Gazebo world
    empty_world_launch = IncludeLaunchDescription(
        PathJoinSubstitution([
            FindPackageShare('ros_gz_sim'),
            'launch',
            'gz_sim.launch.py'
        ]),
        launch_arguments={
            'gui': LaunchConfiguration('gui'),
            'pause': 'true',
            'gz_args': '-r empty.sdf'
        }.items()
    )
    
    # Actually includes robot description 
    description_launch = IncludeLaunchDescription(
        PathJoinSubstitution([
            FindPackageShare('urdf_launch'),
            'launch',
            'description.launch.py'
        ]),
        launch_arguments={
            'urdf_package': LaunchConfiguration('urdf_package'),
            'urdf_package_path': LaunchConfiguration('urdf_package_path')
        }.items()
    )
    
    # Spawn the robot in Gazebo
    urdf_spawner_node = Node(
        package='ros_gz_sim',
        executable='create',
        name='urdf_spawner',
        arguments=['-topic', '/robot_description', '-entity', 'armando', '-z', '0.5', '-unpause'],
        output='screen'
    )
    
    # Camera bridge
    camera_bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        name='camera_bridge',
        arguments=[
            '/camera@sensor_msgs/msg/Image@ignition.msgs.Image',
            '/camera_info@sensor_msgs/msg/CameraInfo@ignition.msgs.CameraInfo'
        ],
        output='screen'
    )
    
    # Spawn joint_state_broadcaster (after robot spawns)
    load_joint_state_broadcaster = RegisterEventHandler(
        event_handler=OnProcessExit(
            target_action=urdf_spawner_node,
            on_exit=[
                Node(
                    package='controller_manager',
                    executable='spawner',
                    arguments=['joint_state_broadcaster'],
                    output='screen'
                )
            ]
        )
    )
    
    # Spawn position_controller (after joint_state_broadcaster)
    load_position_controller = Node(
        package='controller_manager',
        executable='spawner',
        arguments=['position_controller', '--controller-manager', '/controller_manager'],
        output='screen',
        # Only run if controller_type is 'position'
        condition=IfCondition(
            EqualsSubstitution(LaunchConfiguration('controller_type'), 'position')
        )
    )
    
    load_trajectory_controller = Node(
    	package='controller_manager',
    	executable='spawner',
    	arguments=['joint_trajectory_controller', '--controller-manager', '/controller_manager'],
    	output='screen',
    	# Only run if controller_type is 'trajectory'
        condition=IfCondition(
            EqualsSubstitution(LaunchConfiguration('controller_type'), 'trajectory')
        )
     )
    
    return LaunchDescription([
        gui_arg,
        urdf_package_arg,
        urdf_path_arg,
        controller_type_arg,
        empty_world_launch,
        description_launch,
        urdf_spawner_node,
        camera_bridge,  # Camera bridge added here
        load_joint_state_broadcaster,  
        load_position_controller,
        load_trajectory_controller    
    ])
