#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <list>
#include <thread>

#include "application_nodes.h"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp/experimental/fifo_sched.hpp"

#include "utils.h"

using cactus_rt::tracing::ThreadTracer;
using camera_demo_interfaces::msg::FakeImage;

CameraProcessingNode::CameraProcessingNode(
  std::shared_ptr<ThreadTracer> tracer
) : Node("obj_detect"), tracer_(tracer) {
  callback_group_ = this->create_callback_group(rclcpp::CallbackGroupType::Reentrant);
  rclcpp::SubscriptionOptions subscription_options;
  subscription_options.callback_group = callback_group_;

  subscription_data_logger_ = this->create_subscription<FakeImage>(
    "/image",
    10,
    std::bind(&CameraProcessingNode::DataLoggerCallback, this, std::placeholders::_1),
    subscription_options
  );

  subscription_object_detector_ = this->create_subscription<FakeImage>(
    "/image",
    10,
    std::bind(&CameraProcessingNode::ObjectDetectorCallback, this, std::placeholders::_1),
    subscription_options
  );

  publisher_ = this->create_publisher<std_msgs::msg::Int64>("/actuation", 10);

  // TODO: Omit this in the exercise
  sched_param sp;
  sp.sched_priority = HIGH;
  subscription_object_detector_->sched_param(sp);
}

void CameraProcessingNode::ObjectDetectorCallback(const FakeImage::SharedPtr image) {
  // A hack to trace the message passing delay between publisher and this node
  auto now = cactus_rt::NowNs();
  tracer_->StartSpan("MessageDelay", nullptr, image->published_at_monotonic_nanos);
  tracer_->EndSpan(now);

  {
    auto span = tracer_->WithSpan("ObjectDetect");

    // Pretend it takes 4000 ms to do object detection.
    WasteTime(std::chrono::microseconds(4000));

    // Send a signal to the downstream actuation node
    std_msgs::msg::Int64 msg;
    msg.data = image->published_at_monotonic_nanos;
    publisher_->publish(msg);
  }
}

void CameraProcessingNode::DataLoggerCallback(const FakeImage::SharedPtr image) {
  auto span = tracer_->WithSpan("DataLogger");

  // Assume it takes 1ms to serialize the data which is all on the CPU
  WasteTime(std::chrono::microseconds(1000));

  // Assume it takes about 1ms to write the data where it is blocking but yielded to the CPU.
  std::this_thread::sleep_for(std::chrono::microseconds(1000));
}

ActuationNode::ActuationNode(
  std::shared_ptr<cactus_rt::tracing::ThreadTracer> tracer
) : Node("actuation"), tracer_(tracer) {
  subscription_ = this->create_subscription<std_msgs::msg::Int64>(
    "/actuation",
    10,
    std::bind(&ActuationNode::MessageCallback, this, std::placeholders::_1)
  );

  // TODO: Omit this in the exercise
  sched_param sp;
  sp.sched_priority = HIGH;
  subscription_->sched_param(sp);
}

void ActuationNode::MessageCallback(const std_msgs::msg::Int64::SharedPtr published_at_timestamp) {
  auto now = cactus_rt::NowNs();

  // A hack to show the end to end latency, from the moment it was published to
  // the moment it is received by this node.
  tracer_->StartSpan("EndToEndDelay", nullptr, published_at_timestamp->data);
  tracer_->EndSpan(now);

  {
    auto span = tracer_->WithSpan("Actuation");
    WasteTime(std::chrono::microseconds(150));
  }
}
