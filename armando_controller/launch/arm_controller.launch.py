from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution, TextSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare

def generate_launch_description():
    controller_type_arg = DeclareLaunchArgument(
        'controller_type',
        default_value='position',
        description='Type of controller to use: position or trajectory'
    )
    
    # Use substitutions to build the filename dynamically
    config_file = PathJoinSubstitution([
        FindPackageShare('armando_controller'),
        'config',
        [LaunchConfiguration('controller_type'), TextSubstitution(text='_commands.yaml')]
    ])
    
    arm_controller_node = Node(
        package='armando_controller',
        executable='arm_controller_node',
        name='arm_controller_node',
        output='screen',
        parameters=[
            config_file,
            {'controller_type': LaunchConfiguration('controller_type')}
        ]
    )
    
    return LaunchDescription([
        controller_type_arg,
        arm_controller_node
    ])
