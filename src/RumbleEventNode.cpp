#include <linux/input.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <ros/ros.h>
#include <sensor_msgs/JoyFeedbackArray.h>


class RumbleEventNode {
public:
  RumbleEventNode() :
      rumble_fd(-1),
      device("/usr/input/gamepads/event_dft"),
      duration_sec(5.0),
      local_handle("~") {
    // Initialize internal structures
    memset(&rumble_effect, 0, sizeof(rumble_effect));
    rumble_effect.type = FF_RUMBLE;
    rumble_effect.id = -1; // Will get populated by ioctl
    memset(&rumble_event, 0, sizeof(rumble_event));
    rumble_event.type = EV_FF;
    rumble_event.code = -1;
    
    // Update ROS parameters
    local_handle.param<std::string>("device", device, device);
    local_handle.param<double>("duration_sec", duration_sec, duration_sec);
    
    // Setup subscribers
    set_rumble_sub = local_handle.subscribe("set_rumble", 10,
      &RumbleEventNode::setRumbleCB, this);
  };
  
  
  ~RumbleEventNode() {
    termRumbleDevice();
  };


  bool initRumbleDevice() {
    rumble_fd = open(device.c_str(), O_RDWR);
    if (rumble_fd == -1) {
      ROS_ERROR_STREAM("Failed to open " << device);
      return false;
    }
    return true;
  };
  
  
  void setRumbleCB(const sensor_msgs::JoyFeedbackArray::ConstPtr& msg) {
    double strongMagnPct = 0, weakMagnPct = 0;
    bool update = false;
    for (const sensor_msgs::JoyFeedback& f: msg->array) {
      if (f.type == sensor_msgs::JoyFeedback::TYPE_RUMBLE) {
        if (f.id == 0) { strongMagnPct = f.intensity; update = true; }
        else if (f.id == 1) { weakMagnPct = f.intensity; update = true; }
      }
    }
    
    if (update) {
      setRumble(strongMagnPct, weakMagnPct, duration_sec);
    }
  };
  
  
  void spin() {
    ros::spin();
  };

  
protected:
  void termRumbleDevice() {
    if (rumble_fd >= 0) {
      issueRumbleEvent(false); // Stop previous rumble effect
      close(rumble_fd);
      rumble_fd = -1;
    }
  };
  
  
  void issueRumbleEvent(bool active) {
    if (rumble_fd < 0 || rumble_effect.id < 0) return;
    
    rumble_event.code = rumble_effect.id;
    rumble_event.value = active;
    if (write(rumble_fd, (const void*) &rumble_event, sizeof(rumble_event)) == -1) {
      ROS_ERROR_STREAM("Failed to issue rumble event");
      return;
    }
  };
  
  
  void setRumble(double strongMagnPct, double weakMagnPct,
      double durationSec, double delaySec=0) {
    if (rumble_fd < 0) return;
        
    // Stop and clear previous rumble effect
    if (rumble_effect.id >= 0) {
      issueRumbleEvent(false);
      
      if (ioctl(rumble_fd, EVIOCRMFF, rumble_effect.id) == -1) {
        ROS_ERROR_STREAM("Failed to clear rumble effect");
        return;
      }
      rumble_effect.id = -1;
    }

    // Upload new rumble effect
    rumble_effect.u.rumble.strong_magnitude = 65535*std::min(std::max(strongMagnPct, 0.0), 1.0);
    rumble_effect.u.rumble.weak_magnitude = 65535*std::min(std::max(weakMagnPct, 0.0), 1.0);
    rumble_effect.replay.length = std::min(std::max(durationSec, 0.0), 65.535)*1000;
    rumble_effect.replay.delay = std::min(std::max(delaySec, 0.0), 65.535)*1000;
    if (ioctl(rumble_fd, EVIOCSFF, &rumble_effect) == -1) {
      ROS_ERROR_STREAM("Failed to upload rumble effect");
      return;
    }
    
    // Start rumble
    issueRumbleEvent(true);
  };


  int rumble_fd;
  struct ff_effect rumble_effect;
  struct input_event rumble_event;
  std::string device;
  double duration_sec;
  
  ros::NodeHandle local_handle;
  ros::Subscriber set_rumble_sub;
};


int main(int argc, char** argv) {
  ros::init(argc, argv, "rumble_event_node");
  RumbleEventNode* n = new RumbleEventNode();
  if (n->initRumbleDevice()) {
    n->spin();
  }
  delete n;
  
  return EXIT_SUCCESS;
};
