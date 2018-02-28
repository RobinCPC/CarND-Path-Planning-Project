#include <iostream>
#include "vehicle.h"

using std::cout;
using std::endl;

/*
 * Initialize Vehicle
 */

Vehicle::Vehicle(){}

Vehicle::Vehicle(int id, double x, double y, double s, double d, double v, State st)
{
  this->id = id;
  this->x = x;
  this->y = y;
  this->s = s;
  this->d = d;
  this->v = v;
  this->state = st;
  // find out which lane is the car located
  find_lane(d);

  this->front_dist = 999.0;
  this->left_front_dist = 999.0;
  this->right_front_dist = 999.0;
}


Vehicle::~Vehicle(){}


void Vehicle::find_lane(double d, int lane_width, int num_lane)
{

  this->lane = 0;
  int i=0;
  while(i < num_lane)
  {
    if( d >= i*lane_width && d < (i+1)*lane_width)
    {
      this->lane = i;
    }
    ++i;
  }

  if(d > num_lane*lane_width || d < 0)
  {
    cout << "car:" << this->id << " run outside of lane!\n";
    cout << "car:" << this->id << " at d = " << this->d;

  }

  return;
}


vector<State> Vehicle::successor_states()
{
  vector<State> states;
  states.push_back(State::KL);

  State cur_state = this->state;
  int cur_lane = this->lane;

  if(cur_state == State::KL)
  {
    if(cur_lane > 0)
    {
      states.push_back(State::LCL);
    }

    if(cur_lane < 2)
    {
      states.push_back(State::LCR);
    }
  }

  // If state is "LCL" or "LCR", then just return "KL"
  return states;
}


