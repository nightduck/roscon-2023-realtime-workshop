#include "application_nodes.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "utils.h"

using cactus_rt::tracing::ThreadTracer;
using camera_demo_interfaces::msg::FakeImage;

CameraProcessingNode::CameraProcessingNode(
  std::shared_ptr<ThreadTracer> tracer_object_detector,
  std::shared_ptr<ThreadTracer> tracer_data_logger
) : Node("obj_detect"), tracer_object_detector_(tracer_object_detector), tracer_data_logger_(tracer_data_logger) {
  subscription_data_logger_ = this->create_subscription<FakeImage>(
    "/image",
    10,
    std::bind(&CameraProcessingNode::DataLoggerCallback, this, std::placeholders::_1)
  );

  subscription_object_detector_ = this->create_subscription<FakeImage>(
    "/image",
    10,
    std::bind(&CameraProcessingNode::ObjectDetectorCallback, this, std::placeholders::_1)
  );

  publisher_ = this->create_publisher<std_msgs::msg::Int64>("/actuation", 10);

  // initialization of latency random variable
  srand((unsigned)time(NULL));
}

void CameraProcessingNode::ObjectDetectorCallback(const FakeImage::SharedPtr image) {
  auto span = tracer_object_detector_->WithSpan("ObjectDetect");

  // Pretend it takes 3ms to do object detection.
  WasteTime(std::chrono::microseconds(3000));

  // Send a signal to the downstream actuation node
  std_msgs::msg::Int64 msg;
  msg.data = image->published_at_monotonic_nanos;
  publisher_->publish(msg);
}

void CameraProcessingNode::DataLoggerCallback(const FakeImage::SharedPtr image) {
  auto span = tracer_data_logger_->WithSpan("DataLogger");

  // Assume it takes 6ms to serialize the data which is all on the CPU
  // random number between [6ms,15ms]
  // 15 ms + 3ms (objectDetector) = 18 ms > publisher rate (16.7ms)

  unsigned int data_logger_latency = 6000 + (rand() % 9001);
  // std::cout << "latency " << data_logger_latency << std::endl;
  WasteTime(std::chrono::microseconds(data_logger_latency));

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
}

void ActuationNode::MessageCallback(const std_msgs::msg::Int64::SharedPtr published_at_timestamp) {
  auto span = tracer_->WithSpan("Actuation");
  WasteTime(std::chrono::microseconds(150));
}
