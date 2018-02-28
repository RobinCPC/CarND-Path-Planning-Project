#ifndef VEHICLE_H
#define VEHICLE_H
#include <vector>

using std::vector;

enum State{
  KL = 0,
  LCL,
  LCR
};

class Vehicle
{
public:
  int id;     // the unique ID of the car
  double x;   // position in the world coordination
  double y;
  double s;   // position in the frenet coordiation
  double d;
  double v;   // speed
  double yaw; // the orientation of car
  int lane;   // current position in whcih lane

  State state;

  // info to check
  double front_dist;
  double left_front_dist;
  double right_front_dist;
  

  /*
   * Constructor
   */
  Vehicle();
  Vehicle(int id, double x, double y, double s, double d, double v, State st=KL);

  /*
   * Destructor
   */
  virtual ~Vehicle();

  // find out the lane is in which lane
  void find_lane(double d, int lane_width=4, int num_lane=3);

  // Provides the possible next states given the current state for the FSM
  vector<State> successor_states();

  
};

#endif
