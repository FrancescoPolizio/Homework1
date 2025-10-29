#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <std_msgs/msg/float64_multi_array.hpp>
#include <trajectory_msgs/msg/joint_trajectory.hpp>
#include <trajectory_msgs/msg/joint_trajectory_point.hpp>
#include <vector>
#include <chrono>
#include <string>
#include "rclcpp/qos.hpp"

using namespace std::chrono_literals;

class ArmControllerNode : public rclcpp::Node
{
public:
    ArmControllerNode() : Node("arm_controller_node"), command_index_(0)
    {
        if (!loadAndValidateParameters())
        {
            RCLCPP_ERROR(this->get_logger(), "Failed to initialize. Shutting down.");
            return;
        }
        setupCommunications();
        RCLCPP_INFO(this->get_logger(), "Arm Controller Node started successfully.");
    }

private:
    //Parameter Loading 
    
    bool loadAndValidateParameters()
    {
        if (!declareAndLoadParameters())
        {
            return false;
        }
        
        if (!validateParameters())
        {
            return false;
        }
        
        if (!reconstructCommandsFromFlatArray())
        {
            return false;
        }
        
        logConfigurationSummary();
        return true;
    }

    bool declareAndLoadParameters()
    {
        //define parameters and their default values
        this->declare_parameter("controller_type", "position");
        this->declare_parameter("command_publish_period", 1.0);
        this->declare_parameter("position_commands_flat", std::vector<double>{});
        this->declare_parameter("num_commands", 0);
        this->declare_parameter("joints_per_command", 4);
        
        // Trajectory-specific parameters
        this->declare_parameter("trajectory_duration", 5.0);

        //actually initialize parameters with those provided by the user if they are provided (otherwise default)
        controller_type_ = this->get_parameter("controller_type").as_string();
        publish_period_ = this->get_parameter("command_publish_period").as_double();
        flat_commands_ = this->get_parameter("position_commands_flat").as_double_array();
        num_commands_ = this->get_parameter("num_commands").as_int();
        joints_per_command_ = this->get_parameter("joints_per_command").as_int();
        //Trajectory-specific
        trajectory_duration_ = this->get_parameter("trajectory_duration").as_double();
        
        return true;
    }

    bool validateParameters()
    {
        if (num_commands_ <= 0 || joints_per_command_ <= 0)
        {
            RCLCPP_ERROR(this->get_logger(),
                "Invalid dimensions: num_commands=%d, joints_per_command=%d",
                num_commands_, joints_per_command_);
            return false;
        }

        //Checks if: The size of flat_commands_ (a 1D array) matches the expected total number of elements based on a 2D structure.
        if (flat_commands_.size() != static_cast<size_t>(num_commands_ * joints_per_command_))
        {
            RCLCPP_ERROR(this->get_logger(),
                "Flat array size (%zu) doesn't match dimensions (%d x %d = %d)",
                flat_commands_.size(), num_commands_, joints_per_command_,
                num_commands_ * joints_per_command_);
            return false;
        }

        if (controller_type_ != "position" && controller_type_ != "trajectory")
        {
            RCLCPP_ERROR(this->get_logger(),
                "Invalid controller_type '%s'. Must be 'position' or 'trajectory'",
                controller_type_.c_str());
            return false;
        }

        if (num_commands_ < 4) //Specified requirement
        {
            RCLCPP_ERROR(this->get_logger(),
                "Configuration file must contain at least 4 position commands! Found: %d",
                num_commands_);
            return false;
        }
        
        
        return true;
    }

    /* This function converts a 1D (flat) array into a 2D array structure.
    Description:
        Input: flat_commands_ - a 1D array containing all commands in sequence
        Output: position_commands_ - a 2D array organized as [command_index][joint_index] */

    /* Example:
    If one has:

    num_commands_ = 3 (3 waypoints)
    joints_per_command_ = 2 (2 joints)
    flat_commands_ = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0]

    Result:
        position_commands_[0] = [1.0, 2.0]  // Command 0: joint0=1.0, joint1=2.0
        position_commands_[1] = [3.0, 4.0]  // Command 1: joint0=3.0, joint1=4.0
        position_commands_[2] = [5.0, 6.0]  // Command 2: joint0=5.0, joint1=6.0
    */

    bool reconstructCommandsFromFlatArray()
    {
        position_commands_.resize(num_commands_);
        for (int i = 0; i < num_commands_; i++)
        {
            position_commands_[i].resize(joints_per_command_);
            for (int j = 0; j < joints_per_command_; j++)
            {
                position_commands_[i][j] = flat_commands_[i * joints_per_command_ + j];//the last operation is used 
                // for the conversion from 2D coordinates to a 1D index
            }
        }
        
        
        return true;
    }

    void logConfigurationSummary()
    {
        RCLCPP_INFO(this->get_logger(),
            "Loaded %zu position commands with %d joint values each.",
            position_commands_.size(), joints_per_command_);

        RCLCPP_INFO(this->get_logger(), "Using controller type: %s", controller_type_.c_str());
        
        if (controller_type_ == "trajectory")
        {
            RCLCPP_INFO(this->get_logger(), "Trajectory duration: %.2f seconds", trajectory_duration_);
        }

        if (!position_commands_.empty())
        {
            std::string first_cmd = formatVectorAsString(position_commands_[0]);
            RCLCPP_WARN(this->get_logger(), "Command 1 (index 0) loaded as: %s", first_cmd.c_str());
            RCLCPP_WARN(this->get_logger(), 
                "Node will publish this first. The next command (index 1) will publish after %.1f seconds.", 
                publish_period_);
        }
    }

    // Communication Setup 

    void setupCommunications()
    {
        //these are defined just after
        setupSubscribers();
        setupPublishers();
        setupTimer();
    }

    void setupSubscribers()
    {
        joint_state_subscriber_ = this->create_subscription<sensor_msgs::msg::JointState>(
            "/joint_states",
            rclcpp::SensorDataQoS(),
            std::bind(&ArmControllerNode::jointStateCallback, this, std::placeholders::_1)
        );
    }

    void setupPublishers()
    {
        if (controller_type_ == "position")//for position controller
        {
            position_command_publisher_ = this->create_publisher<std_msgs::msg::Float64MultiArray>(
                "/position_controller/commands", 10
            );
            RCLCPP_INFO(this->get_logger(), "Position controller publisher created");
        }
        else if (controller_type_ == "trajectory")// for trajectory controller
        {
            trajectory_command_publisher_ = this->create_publisher<trajectory_msgs::msg::JointTrajectory>(
                "/joint_trajectory_controller/joint_trajectory", 10
            );
            RCLCPP_INFO(this->get_logger(), "Trajectory controller publisher created");
        }
    }

    void setupTimer()
    {
        auto period = std::chrono::duration<double>(publish_period_);
        command_timer_ = this->create_wall_timer(
            std::chrono::duration_cast<std::chrono::milliseconds>(period),
            std::bind(&ArmControllerNode::publishNextCommand, this)
        );
        RCLCPP_INFO(this->get_logger(),
            "Command timer started. Publishing every %.1f seconds", publish_period_);
    }

    // Callback 

    void jointStateCallback(const sensor_msgs::msg::JointState::SharedPtr msg)
    {
        //defined just after
        storeJointNames(msg);
        storeCurrentJointPositions(msg);
        logCurrentJointPositions(msg);
    }

    void storeJointNames(const sensor_msgs::msg::JointState::SharedPtr& msg)
    {
        if (joint_names_.empty() && !msg->name.empty())
        {
            joint_names_ = msg->name;
            RCLCPP_INFO(this->get_logger(), "Stored %zu joint names from joint_states", joint_names_.size());
        }
    }

    void storeCurrentJointPositions(const sensor_msgs::msg::JointState::SharedPtr& msg)
    {
        if (!msg->position.empty())
        {
            current_joint_positions_ = msg->position;
        }
    }

    void logCurrentJointPositions(const sensor_msgs::msg::JointState::SharedPtr& msg)
    {
        RCLCPP_INFO(this->get_logger(), "Current Joint Positions:");
        for (size_t i = 0; i < msg->name.size(); i++)
        {
            RCLCPP_INFO(this->get_logger(), " %s: %.4f",
                msg->name[i].c_str(), msg->position[i]);
        }
        RCLCPP_INFO(this->get_logger(), "---");
    }

    // Command Publishing

    void publishNextCommand()
    {
        // Step 1: Publish the command at the CURRENT index
        //The two functions are defined after
        if (controller_type_ == "position")
        {
            publishPositionCommand(); // Uses current command_index_
        }
        else if (controller_type_ == "trajectory")
        {
            publishTrajectoryCommand(); // Uses current command_index_
        }

        // Step 2: Check if this was the LAST command
        
        // Example: 4 commands. Indices are 0, 1, 2, 3.
        // position_commands_.size() is 4.
        // The last index is 3 (i.e., size - 1).
        
        if (command_index_ == position_commands_.size() - 1)
        {
            // This was the last command. Stop the timer.
            RCLCPP_INFO(this->get_logger(),
                "Final command (%zu) published. Trajectory complete. Stopping timer. ",
                command_index_ + 1);
                
            command_timer_->cancel(); // This stops the timer from firing again
        }
        else
        {
            // This was not the last command. Increment the index for the next run.
            command_index_++;
        }
    }

    void publishPositionCommand()
    {
        auto command_msg = std_msgs::msg::Float64MultiArray();
        command_msg.data = position_commands_[command_index_];
        position_command_publisher_->publish(command_msg); //set in the setup section

        RCLCPP_INFO(this->get_logger(),
            "[POSITION] Publishing command %zu/%zu with %zu joint values",
            command_index_ + 1, position_commands_.size(), command_msg.data.size());
        printCommandValues(command_msg.data);
    }

    void publishTrajectoryCommand()
    {
        auto trajectory_msg = trajectory_msgs::msg::JointTrajectory();
        trajectory_msg.joint_names = getJointNames();

        addTrajectoryPoints(trajectory_msg);

        trajectory_command_publisher_->publish(trajectory_msg);//set in the setup section

        RCLCPP_INFO(this->get_logger(),
            "[TRAJECTORY] Publishing command %zu/%zu with %zu joint values",
            command_index_ + 1, position_commands_.size(), 
            trajectory_msg.points.back().positions.size());
        printCommandValues(trajectory_msg.points.back().positions);
    }

    std::vector<std::string> getJointNames()
    {
        if (!joint_names_.empty())
        {
            return joint_names_;
        }

        // Generate default joint names
        std::vector<std::string> default_names(position_commands_[command_index_].size());
        for (size_t i = 0; i < default_names.size(); i++)
        {
            default_names[i] = "j" + std::to_string(i + 1);
        }
        RCLCPP_WARN(this->get_logger(),
            "No joint names from joint_states, using default names");
        return default_names;
    }

    void addTrajectoryPoints(trajectory_msgs::msg::JointTrajectory& trajectory_msg)
    {
        // Goal point with position from config
        trajectory_msgs::msg::JointTrajectoryPoint goal_point;
        goal_point.positions = position_commands_[command_index_];
        
        // Use trajectory duration from config
        goal_point.time_from_start = rclcpp::Duration::from_seconds(trajectory_duration_);

        trajectory_msg.points.push_back(goal_point);
    }

    // Utility Functions

    //This function logs/prints a vector of double values to the console in a readable format.
    void printCommandValues(const std::vector<double>& values)
    {
        std::string values_str = formatVectorAsString(values);
        RCLCPP_INFO(this->get_logger(), " Values: %s", values_str.c_str());
    }
    //This function converts a vector of doubles into a formatted string for display purposes.
    std::string formatVectorAsString(const std::vector<double>& vec)
    {
        std::string result = "[";
        for (size_t i = 0; i < vec.size(); i++)
        {
            result += std::to_string(vec[i]);
            if (i < vec.size() - 1)
            {
                result += ", ";
            }
        }
        result += "]";
        return result;
    }

    // Member Variables of the class

    // ROS Communications //Pointers to ROS 2 objects (subscribers, publishers, timers)
    rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr joint_state_subscriber_;
    rclcpp::Publisher<std_msgs::msg::Float64MultiArray>::SharedPtr position_command_publisher_;
    rclcpp::Publisher<trajectory_msgs::msg::JointTrajectory>::SharedPtr trajectory_command_publisher_;
    rclcpp::TimerBase::SharedPtr command_timer_;

    // Parameters //Configuration values loaded from launch files or parameter files
    std::string controller_type_;
    double publish_period_;
    std::vector<double> flat_commands_;
    int num_commands_;
    int joints_per_command_;
    std::vector<std::vector<double>> position_commands_;
    
    // Trajectory-specific parameters
    double trajectory_duration_;
    

    // State //Current runtime data (joint names, positions, command index)
    std::vector<std::string> joint_names_;
    std::vector<double> current_joint_positions_;
    size_t command_index_;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<ArmControllerNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
