# This is a small description on how to run the code.    
## Preliminaries    

Please note that also a dockerfile was provided since it is needed in order to load the meshes correctly.


It is needed to change the name of the folder from 'Homework1' to 'myArmando' since we used this one as the folder of our packages    
   

It is then needed to build all the three packages in the repo in order to run the code correctly    

 `colcon build`

and to source the environment    

`. install/setup.bash`


    
## How to run the visual part of the homework (RVIZ)
We provided a launch file that starts the environment and loads the robot structure.   
This launch file also supports an additional argument 'use_sim' which specifies whether to use Rviz alongside Gazebo (it was just meant to make Rviz's robot follow the motion of Gazebo's robot) or as a standalone visualizer (use_sim:=false).  
Here are examples of running command line   

` ros2 launch armadno_description armando_display.launch.py use_sim:=false` (default condition)    

` ros2 launch armadno_description armando_display.launch.py use_sim:=true`   (condition to use Rviz alongside Gazebo)   

In addition from the rviz GUI it is possible to switch from the mesh visualization to the collision boxes visualization.     

## How to run the gazebo simulation   
We provided a launch file for this purpose. In this case the argument 'controller_type' should be specified. It's values can be 'position' (default) or 'trajectory'.  
They are needed since in this case we are defining the type of controller we are gonna use in the following control part (since they are closely related)  

` ros2 launch armando_gazebo armando_world.launch.py controller_type:=position `    

`ros2 launch armando_gazebo armando_world.launch.py controller_type:=trajectory`  


## How to start the controller
Even in this case we need to use a launch file and also in this case the controller type should be specified:    

`ros2 launch armando_controller arm_controller.launch.py controller_type:=position`    

`ros2 launch armando_controller arm_controller.launch.py controller_type:=trajectory`  

