// Copyright 2020 Yutaka Kondo <yutaka.kondo@youtalk.jp>
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

#include "dynamixel_hardware/dynamixel_hardware.hpp"

#include <array>
#include <limits>
#include <string>

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"

namespace dynamixel_hardware
{
constexpr const char * kDynamixelHardware = "DynamixelHardware";
constexpr uint8_t kGoalPositionIndex = 0;
constexpr uint8_t kPresentPositionVelocityCurrentIndex = 0;
constexpr const char * kGoalPositionItem = "Goal_Position";
constexpr const char * kGoalVelocityItem = "Goal_Velocity";
constexpr const char * kMovingSpeedItem = "Moving_Speed";
constexpr const char * kPresentPositionItem = "Present_Position";
constexpr const char * kPresentVelocityItem = "Present_Velocity";
constexpr const char * kPresentSpeedItem = "Present_Speed";
constexpr const char * kPresentCurrentItem = "Present_Current";
constexpr const char * kPresentLoadItem = "Present_Load";

std::vector<std::string> split(const std::string & string, const std::string & delimiter)
{
  auto first = 0u;
  auto last = string.find_first_of(delimiter);
  std::vector<std::string> result;

  while (first < string.size()) {
    result.emplace_back(string, first, last - first);

    first = last + 1;
    last = string.find_first_of(delimiter, first);

    if (last == std::string::npos) {
      last = string.size();
    }
  }

  return result;
}

return_type DynamixelHardware::configure(const hardware_interface::HardwareInfo & info)
{
  if (configure_default(info) != return_type::OK) {
    return return_type::ERROR;
  }

  auto joint_ids = split(info_.hardware_parameters["joint_ids"], ", ");
  joints_.resize(info_.joints.size(), Joint());
  joint_ids_.resize(info_.joints.size(), 0);

  if (joint_ids.size() != joints_.size()) {
    RCLCPP_FATAL(rclcpp::get_logger(kDynamixelHardware), "Invalid joint_ids size");
    return return_type::ERROR;
  }

  for (uint i = 0; i < info_.joints.size(); i++) {
    joint_ids_[i] = std::stoi(joint_ids[i]);
    joints_[i].state.position = std::numeric_limits<double>::quiet_NaN();
    joints_[i].state.velocity = std::numeric_limits<double>::quiet_NaN();
    joints_[i].state.effort = std::numeric_limits<double>::quiet_NaN();
    joints_[i].command.position = std::numeric_limits<double>::quiet_NaN();
    joints_[i].command.velocity = std::numeric_limits<double>::quiet_NaN();
    joints_[i].command.effort = std::numeric_limits<double>::quiet_NaN();
  }

  auto usb_port = info_.hardware_parameters["usb_port"];
  auto baud_rate = std::stoi(info_.hardware_parameters["baud_rate"]);
  const char * log = nullptr;

  if (!dynamixel_workbench_.init(usb_port.c_str(), baud_rate, &log)) {
    RCLCPP_FATAL(rclcpp::get_logger(kDynamixelHardware), "%s", log);
    return return_type::ERROR;
  }

  for (uint i = 0; i < info_.joints.size(); ++i) {
    uint16_t model_number = 0;
    if (!dynamixel_workbench_.ping(joint_ids_[i], &model_number, &log)) {
      RCLCPP_FATAL(rclcpp::get_logger(kDynamixelHardware), "%s", log);
      return return_type::ERROR;
    }

    dynamixel_workbench_.torqueOff(joint_ids_[i]);
  }

  for (uint i = 0; i < info_.joints.size(); ++i) {
    dynamixel_workbench_.torqueOn(joint_ids_[i]);
  }

  const ControlItem * goal_position =
    dynamixel_workbench_.getItemInfo(joint_ids_[0], kGoalPositionItem);
  if (goal_position == nullptr) {
    return return_type::ERROR;
  }

  const ControlItem * goal_velocity =
    dynamixel_workbench_.getItemInfo(joint_ids_[0], kGoalVelocityItem);
  if (goal_velocity == nullptr) {
    goal_velocity = dynamixel_workbench_.getItemInfo(joint_ids_[0], kMovingSpeedItem);
  }
  if (goal_velocity == nullptr) {
    return return_type::ERROR;
  }

  const ControlItem * present_position =
    dynamixel_workbench_.getItemInfo(joint_ids_[0], kPresentPositionItem);
  if (present_position == nullptr) {
    return return_type::ERROR;
  }

  const ControlItem * present_velocity =
    dynamixel_workbench_.getItemInfo(joint_ids_[0], kPresentVelocityItem);
  if (present_velocity == nullptr) {
    present_velocity = dynamixel_workbench_.getItemInfo(joint_ids_[0], kPresentSpeedItem);
  }
  if (present_velocity == nullptr) {
    return return_type::ERROR;
  }

  const ControlItem * present_current =
    dynamixel_workbench_.getItemInfo(joint_ids_[0], kPresentCurrentItem);
  if (present_current == nullptr) {
    present_current = dynamixel_workbench_.getItemInfo(joint_ids_[0], kPresentLoadItem);
  }
  if (present_current == nullptr) {
    return return_type::ERROR;
  }

  control_items_[kGoalPositionItem] = goal_position;
  control_items_[kGoalVelocityItem] = goal_velocity;
  control_items_[kPresentPositionItem] = present_position;
  control_items_[kPresentVelocityItem] = present_velocity;
  control_items_[kPresentCurrentItem] = present_current;

  if (!dynamixel_workbench_.addSyncWriteHandler(
        control_items_[kGoalPositionItem]->address, control_items_[kGoalPositionItem]->data_length,
        &log)) {
    RCLCPP_FATAL(rclcpp::get_logger(kDynamixelHardware), "%s", log);
    return return_type::ERROR;
  }

  if (!dynamixel_workbench_.addSyncWriteHandler(
        control_items_[kGoalVelocityItem]->address, control_items_[kGoalVelocityItem]->data_length,
        &log)) {
    RCLCPP_FATAL(rclcpp::get_logger(kDynamixelHardware), "%s", log);
    return return_type::ERROR;
  }

  uint16_t start_address = std::min(
    control_items_[kPresentPositionItem]->address, control_items_[kPresentCurrentItem]->address);
  uint16_t read_length = control_items_[kPresentPositionItem]->data_length +
                         control_items_[kPresentVelocityItem]->data_length +
                         control_items_[kPresentCurrentItem]->data_length + 2;
  if (!dynamixel_workbench_.addSyncReadHandler(start_address, read_length, &log)) {
    RCLCPP_FATAL(rclcpp::get_logger(kDynamixelHardware), "%s", log);
    return return_type::ERROR;
  }

  status_ = hardware_interface::status::CONFIGURED;
  return return_type::OK;
}

std::vector<hardware_interface::StateInterface> DynamixelHardware::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> state_interfaces;
  for (uint i = 0; i < info_.joints.size(); i++) {
    state_interfaces.emplace_back(hardware_interface::StateInterface(
      info_.joints[i].name, hardware_interface::HW_IF_POSITION, &joints_[i].state.position));
    state_interfaces.emplace_back(hardware_interface::StateInterface(
      info_.joints[i].name, hardware_interface::HW_IF_VELOCITY, &joints_[i].state.velocity));
    state_interfaces.emplace_back(hardware_interface::StateInterface(
      info_.joints[i].name, hardware_interface::HW_IF_EFFORT, &joints_[i].state.effort));
  }

  return state_interfaces;
}

std::vector<hardware_interface::CommandInterface> DynamixelHardware::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> command_interfaces;
  for (uint i = 0; i < info_.joints.size(); i++) {
    command_interfaces.emplace_back(hardware_interface::CommandInterface(
      info_.joints[i].name, hardware_interface::HW_IF_POSITION, &joints_[i].command.position));
  }

  return command_interfaces;
}

return_type DynamixelHardware::start()
{
  for (uint i = 0; i < joints_.size(); i++) {
    if (std::isnan(joints_[i].state.position)) {
      joints_[i].state.position = 0.0;
      joints_[i].state.velocity = 0.0;
      joints_[i].state.effort = 0.0;
      joints_[i].command.position = 0.0;
    }
  }

  status_ = hardware_interface::status::STARTED;
  return return_type::OK;
}

return_type DynamixelHardware::stop()
{
  status_ = hardware_interface::status::STOPPED;
  return return_type::OK;
}

hardware_interface::return_type DynamixelHardware::read()
{
  std::vector<uint8_t> ids{};
  std::vector<int32_t> positions(info_.joints.size(), 0);
  std::vector<int32_t> velocities(info_.joints.size(), 0);
  std::vector<int32_t> currents(info_.joints.size(), 0);

  std::copy(joint_ids_.begin(), joint_ids_.end(), ids.begin());
  const char * log = nullptr;

  if (!dynamixel_workbench_.syncRead(
        kPresentPositionVelocityCurrentIndex, ids.data(), ids.size(), &log)) {
    RCLCPP_FATAL(rclcpp::get_logger(kDynamixelHardware), "%s", log);
  }

  if (!dynamixel_workbench_.getSyncReadData(
        kPresentPositionVelocityCurrentIndex, ids.data(), ids.size(),
        control_items_[kPresentCurrentItem]->address,
        control_items_[kPresentCurrentItem]->data_length, currents.data(), &log)) {
    RCLCPP_FATAL(rclcpp::get_logger(kDynamixelHardware), "%s", log);
  }

  if (!dynamixel_workbench_.getSyncReadData(
        kPresentPositionVelocityCurrentIndex, ids.data(), ids.size(),
        control_items_[kPresentVelocityItem]->address,
        control_items_[kPresentVelocityItem]->data_length, velocities.data(), &log)) {
    RCLCPP_FATAL(rclcpp::get_logger(kDynamixelHardware), "%s", log);
  }

  if (!dynamixel_workbench_.getSyncReadData(
        kPresentPositionVelocityCurrentIndex, ids.data(), ids.size(),
        control_items_[kPresentPositionItem]->address,
        control_items_[kPresentPositionItem]->data_length, positions.data(), &log)) {
    RCLCPP_FATAL(rclcpp::get_logger(kDynamixelHardware), "%s", log);
  }

  for (uint i = 0; i < ids.size(); i++) {
    joints_.at(ids.at(i) - 1).state.position =
      dynamixel_workbench_.convertValue2Radian(ids.at(i), positions.at(i));
    joints_.at(ids.at(i) - 1).state.velocity =
      dynamixel_workbench_.convertValue2Velocity(ids.at(i), velocities.at(i));
    joints_.at(ids.at(i) - 1).state.effort =
      dynamixel_workbench_.convertValue2Current(currents.at(i));
    joints_.at(ids.at(i) - 1).command.position = joints_.at(ids.at(i) - 1).state.position;
  }

  return return_type::OK;
}

hardware_interface::return_type dynamixel_hardware::DynamixelHardware::write()
{
  std::vector<uint8_t> ids{};
  std::vector<int32_t> positions(info_.joints.size(), 0);

  std::copy(joint_ids_.begin(), joint_ids_.end(), ids.begin());
  const char * log = nullptr;

  for (uint i = 0; i < ids.size(); i++) {
    positions.at(i) = dynamixel_workbench_.convertRadian2Value(
      ids.at(i), static_cast<float>(joints_.at(ids.at(i) - 1).command.position));
  }

  if (!dynamixel_workbench_.syncWrite(
        kGoalPositionIndex, ids.data(), ids.size(), positions.data(), 1, &log)) {
    RCLCPP_FATAL(rclcpp::get_logger(kDynamixelHardware), "%s", log);
  }

  return return_type::OK;
}
}  // namespace dynamixel_hardware

#include "pluginlib/class_list_macros.hpp"

PLUGINLIB_EXPORT_CLASS(dynamixel_hardware::DynamixelHardware, hardware_interface::SystemInterface)
