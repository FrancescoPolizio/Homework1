FROM osrf/ros:humble-desktop

#Uncomment the following line if you get the "release file is not valid yet" error during apt-get
#	(solution from: https://stackoverflow.com/questions/63526272/release-file-is-not-valid-yet-docker)
#RUN echo "Acquire::Check-Valid-Until \"false\";\nAcquire::Check-Date \"false\";" | cat > /etc/apt/apt.conf.d/10no--check-valid-until

#Install essential
RUN apt-get update && apt-get install -y nano 
RUN apt-get update && apt-get install -y wget 
RUN apt-get update && apt-get install -y gedit
RUN apt-get update && apt-get install -y locate
RUN apt-get update && apt-get install -y ros-humble-xacro
RUN apt-get update && apt-get install -y ros-humble-ros-ign-bridge
RUN apt-get update && apt-get install -y ros-humble-urdf-tutorial
RUN apt-get update && apt-get install -y ros-humble-ros-gz
RUN apt-get update && apt-get install -y ros-humble-urdf-launch
RUN apt-get update && apt-get install ros-humble-controller-manager -y 
RUN apt-get update && apt-get install ros-humble-ros2-control -y 
RUN apt-get update && apt-get install ros-humble-ros2-controllers -y 
RUN apt-get update && apt-get install ros-humble-ign-ros2-control -y


#Environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV DISPLAY=:0
ENV HOME=/home/user
ENV ROS_DISTRO=humble

#Add non root user using UID and GID passed as argument
ARG USER_ID
ARG GROUP_ID
RUN addgroup --gid $GROUP_ID user
RUN adduser --disabled-password --gecos '' --uid $USER_ID --gid $GROUP_ID user
RUN echo "user:user" | chpasswd
RUN echo "user ALL=(ALL:ALL) ALL" >> /etc/sudoers
USER user

#ROS2 workspace creation and compilation
RUN mkdir -p ${HOME}/ros2_ws/src
WORKDIR ${HOME}/ros2_ws/
#COPY --chown=user ./src ${HOME}/ros2_ws/src
RUN rosdep update
USER root
RUN rosdep install -i --from-paths src --rosdistro ${ROS_DISTRO} -y 
USER user
SHELL ["/bin/bash", "-c"] 
RUN source /opt/ros/${ROS_DISTRO}/setup.bash; colcon build --symlink-install

#Add script and export env variables to .bashrc
RUN echo "source /opt/ros/${ROS_DISTRO}/setup.bash;" >>  ${HOME}/.bashrc
RUN echo "source ${HOME}/ros2_ws/install/local_setup.bash;" >>  ${HOME}/.bashrc
RUN echo "source /usr/share/colcon_cd/function/colcon_cd.sh" >> ${HOME}/.bashrc
RUN echo "export _colcon_cd_root=/opt/ros/${ROS_DISTRO}/" >> ${HOME}/.bashrc
RUN echo "source /usr/share/colcon_argcomplete/hook/colcon-argcomplete.bash" >> ${HOME}/.bashrc
RUN echo "export GAZEBO_AUDIO=0" >> ${HOME}/.bashrc
RUN echo 'export IGN_GAZEBO_RESOURCE_PATH=/home/user/ros2_ws/src/myArmando:$IGN_GAZEBO_RESOURCE_PATH' >> /home/user/.bashrc
RUN echo 'export LD_LIBRARY_PATH="/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH"' >> ${HOME}/.bashrc
RUN echo 'export IGN_GAZEBO_PLUGIN_PATH="/usr/lib/x86_64-linux-gnu:$IGN_GAZEBO_PLUGIN_PATH"' >> ${HOME}/.bashrc


#Clean image
USER root
RUN rm -rf /var/lib/apt/lists/*
#RUN chown root:user /dev/video0
RUN updatedb
USER user
