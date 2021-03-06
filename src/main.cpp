#include <fstream>
#include <math.h>
#include <uWS/uWS.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "json.hpp"
#include "spline.h"
#include "vehicle.h"

using namespace std;

// for convenience
using json = nlohmann::json;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
// else the empty string "" will be returned.
string hasData(string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.find_first_of("}");
  if (found_null != string::npos) {
    return "";
  } else if (b1 != string::npos && b2 != string::npos) {
    return s.substr(b1, b2 - b1 + 2);
  }
  return "";
}

double distance(double x1, double y1, double x2, double y2)
{
	return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
}
int ClosestWaypoint(double x, double y, const vector<double> &maps_x, const vector<double> &maps_y)
{

	double closestLen = 100000; //large number
	int closestWaypoint = 0;

	for(int i = 0; i < (int)maps_x.size(); i++)
	{
		double map_x = maps_x[i];
		double map_y = maps_y[i];
		double dist = distance(x,y,map_x,map_y);
		if(dist < closestLen)
		{
			closestLen = dist;
			closestWaypoint = i;
		}

	}

	return closestWaypoint;

}

int NextWaypoint(double x, double y, double theta, const vector<double> &maps_x, const vector<double> &maps_y)
{

	int closestWaypoint = ClosestWaypoint(x,y,maps_x,maps_y);

	double map_x = maps_x[closestWaypoint];
	double map_y = maps_y[closestWaypoint];

	double heading = atan2((map_y-y),(map_x-x));

	double angle = fabs(theta-heading);
  angle = min(2*pi() - angle, angle);

  if(angle > pi()/4)
  {
    closestWaypoint++;
  if (closestWaypoint == (int)maps_x.size())
  {
    closestWaypoint = 0;
  }
  }

  return closestWaypoint;
}

// Transform from Cartesian x,y coordinates to Frenet s,d coordinates
vector<double> getFrenet(double x, double y, double theta, const vector<double> &maps_x, const vector<double> &maps_y)
{
	int next_wp = NextWaypoint(x,y, theta, maps_x,maps_y);

	int prev_wp;
	prev_wp = next_wp-1;
	if(next_wp == 0)
	{
		prev_wp  = maps_x.size()-1;
	}

	double n_x = maps_x[next_wp]-maps_x[prev_wp];
	double n_y = maps_y[next_wp]-maps_y[prev_wp];
	double x_x = x - maps_x[prev_wp];
	double x_y = y - maps_y[prev_wp];

	// find the projection of x onto n
	double proj_norm = (x_x*n_x+x_y*n_y)/(n_x*n_x+n_y*n_y);
	double proj_x = proj_norm*n_x;
	double proj_y = proj_norm*n_y;

	double frenet_d = distance(x_x,x_y,proj_x,proj_y);

	//see if d value is positive or negative by comparing it to a center point

	double center_x = 1000-maps_x[prev_wp];
	double center_y = 2000-maps_y[prev_wp];
	double centerToPos = distance(center_x,center_y,x_x,x_y);
	double centerToRef = distance(center_x,center_y,proj_x,proj_y);

	if(centerToPos <= centerToRef)
	{
		frenet_d *= -1;
	}

	// calculate s value
	double frenet_s = 0;
	for(int i = 0; i < prev_wp; i++)
	{
		frenet_s += distance(maps_x[i],maps_y[i],maps_x[i+1],maps_y[i+1]);
	}

	frenet_s += distance(0,0,proj_x,proj_y);

	return {frenet_s,frenet_d};

}

// Transform from Frenet s,d coordinates to Cartesian x,y
vector<double> getXY(double s, double d, const vector<double> &maps_s, const vector<double> &maps_x, const vector<double> &maps_y)
{
	int prev_wp = -1;

	while(s > maps_s[prev_wp+1] && (prev_wp < (int)(maps_s.size()-1) ))
	{
		prev_wp++;
	}

	int wp2 = (prev_wp+1)%maps_x.size();

	double heading = atan2((maps_y[wp2]-maps_y[prev_wp]),(maps_x[wp2]-maps_x[prev_wp]));
	// the x,y,s along the segment
	double seg_s = (s-maps_s[prev_wp]);

	double seg_x = maps_x[prev_wp]+seg_s*cos(heading);
	double seg_y = maps_y[prev_wp]+seg_s*sin(heading);

	double perp_heading = heading-pi()/2;

	double x = seg_x + d*cos(perp_heading);
	double y = seg_y + d*sin(perp_heading);

	return {x,y};

}

int main() {
  uWS::Hub h;

  // Load up map values for waypoint's x,y,s and d normalized normal vectors
  vector<double> map_waypoints_x;
  vector<double> map_waypoints_y;
  vector<double> map_waypoints_s;
  vector<double> map_waypoints_dx;
  vector<double> map_waypoints_dy;

  // Waypoint map to read from
  string map_file_ = "../data/highway_map.csv";
  // The max s value before wrapping around the track back to 0
  double max_s = 6945.554;

  ifstream in_map_(map_file_.c_str(), ifstream::in);

  string line;
  while (getline(in_map_, line)) {
  	istringstream iss(line);
  	double x;
  	double y;
  	float s;
  	float d_x;
  	float d_y;
  	iss >> x;
  	iss >> y;
  	iss >> s;
  	iss >> d_x;
  	iss >> d_y;
  	map_waypoints_x.push_back(x);
  	map_waypoints_y.push_back(y);
  	map_waypoints_s.push_back(s);
  	map_waypoints_dx.push_back(d_x);
  	map_waypoints_dy.push_back(d_y);
  }

  // start in lane 1
  int lane = 1;

  // have a reference volecity to target
  double ref_vel = 0.;  //mph

  h.onMessage([&ref_vel, &map_waypoints_x,&map_waypoints_y,&map_waypoints_s,&map_waypoints_dx,&map_waypoints_dy, &lane](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                     uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    //auto sdata = string(data).substr(0, length);
    //cout << sdata << endl;
    if (length && length > 2 && data[0] == '4' && data[1] == '2') {

      auto s = hasData(data);

      if (s != "") {
        auto j = json::parse(s);
        
        string event = j[0].get<string>();
        
        if (event == "telemetry") {
          // j[1] is the data JSON object
          
        	// Main car's localization Data
          	double car_x = j[1]["x"];
          	double car_y = j[1]["y"];
          	double car_s = j[1]["s"];
          	double car_d = j[1]["d"];
          	double car_yaw = j[1]["yaw"];
          	double car_speed = j[1]["speed"];

          	// Previous path data given to the Planner
          	auto previous_path_x = j[1]["previous_path_x"];
          	auto previous_path_y = j[1]["previous_path_y"];
          	// Previous path's end s and d values 
          	double end_path_s = j[1]["end_path_s"];
          	double end_path_d = j[1]["end_path_d"];

          	// Sensor Fusion Data, a list of all other cars on the same side of the road.
            // a 2D vector of cars state, each row represent one state of car
            // --> [ID, x_map_coor, y_map_coor, x_vel, y_vel, s_fren, d_frent]
          	auto sensor_fusion = j[1]["sensor_fusion"];

            int prev_size = previous_path_x.size();
            // record my own car
            Vehicle ego(211, car_x, car_y, car_s, car_d, car_speed);
            double max_s = 6945.554;
            static double keep_duration = 0.01;
            static int ego_lane_pre = 1;
            if (ego_lane_pre == ego.lane)
            {
              keep_duration += 0.02;
            }
            else
            {
              ego_lane_pre = ego.lane;
              keep_duration = 0.01;
            }

            // TODO: Using the data from sensor_fusion to avoid collision
            if(prev_size > 0)
            {
              car_s = end_path_s;
            }

            bool too_close = false;
            vector<vector<Vehicle>> vec_lane {};
            // only three lanes in this simulation
            for(int i=0; i<3; ++i)
              vec_lane.push_back(vector<Vehicle> {});
            std::system("clear");
            cout << "total lane: " << vec_lane.size() << endl;

            // find out the location of all nearby car
            for (int i = 0; i < (int)sensor_fusion.size(); i++)
            {
              // record nearby cars
              int n_id = sensor_fusion[i][0];
              double nx = sensor_fusion[i][1];
              double ny = sensor_fusion[i][2];
              double nvx = sensor_fusion[i][3];
              double nvy = sensor_fusion[i][4];
              double ns = sensor_fusion[i][5];
              double nd = sensor_fusion[i][6];

              // get combination of vx and vy
              double total_speed = sqrt(nvx*nvx + nvy*nvy);
              // get current frenet s coord. (have latency)
              double cur_s = ns; //+ ( (double)prev_size * .02 * total_speed);
              Vehicle nearCar(n_id, nx, ny, cur_s, nd, total_speed);

              // record nearCar in vec_lane
              vec_lane[nearCar.lane].push_back(nearCar);

            }

            // check all posible next states
            cout << "All possible next states are: \n";
            vector<State> pos_next_states = ego.successor_states();
            vector<double> costs;
            double cost;
            for(auto s : pos_next_states)
            {
              switch (s) 
              {
                case State::KL:
                  {
                    cout << "Keep Lane\n";
                    double front_car_dist = 9999.0;
                    double back_car_dist = 9999.0;
                    if(vec_lane[ego.lane].size() == 0) // no car at front
                    {
                      cost = 0;
                    }else
                    {
                      // find most close car
                      for(auto car : vec_lane[ego.lane])
                      { 
                        double dist = car.s - ego.s;
                        if( dist >=0 && dist < front_car_dist)
                        {
                          front_car_dist = dist;
                        }else if( dist < 0 && fabs(dist) < back_car_dist)
                        {
                          back_car_dist = fabs(dist);
                        }

                      }
                      ego.front_dist = front_car_dist;
                      cost = (60 / front_car_dist);

                    }
                    cout << "cost: " << cost << endl;
                    costs.push_back(cost);
                  }
                  break;
                case State::LCL:
                  {
                    cout << "Lane Change Left\n";
                    double front_car_dist = 9999.0;
                    double back_car_dist = 9999.0;
                    int ln = ego.lane -1;
                    if(vec_lane[ln].size() == 0) // no car at front
                    {
                      cost = 0;
                    }else
                    {
                      // find most close car
                      for(auto car : vec_lane[ln])
                      { 
                        double dist = car.s - ego.s;
                        if( dist >=0 && dist < front_car_dist)
                        {
                          front_car_dist = dist;
                        }else if( dist < 0 && fabs(dist) < back_car_dist)
                        {
                          back_car_dist = fabs(dist);
                        }

                      }
                      ego.left_front_dist = front_car_dist;
                      cost = (30 / front_car_dist) + (30 / back_car_dist);
                      // panelty to change lane quickly
                      //if(keep_duration <= 1.)
                      cost += (1.5 / keep_duration);

                    }
                    cout << "cost: " << cost << endl;
                    costs.push_back(cost);
                  }
                  break;
                case State::LCR:
                  {
                    cout << "Lane Change Right\n";
                    double front_car_dist = 9999.0;
                    double back_car_dist = 9999.0;
                    int ln = ego.lane + 1;
                    if(vec_lane[ln].size() == 0) // no car at front
                    {
                      cost = 0;
                    }else
                    {
                      // find most close car
                      for(auto car : vec_lane[ln])
                      { 
                        double dist = car.s - ego.s;
                        if( dist >=0 && dist < front_car_dist)
                        {
                          front_car_dist = dist;
                        }else if( dist < 0 && fabs(dist) < back_car_dist)
                        {
                          back_car_dist = fabs(dist);
                        }

                      }
                      ego.right_front_dist = front_car_dist;
                      cost = (30 / front_car_dist) + (30 / back_car_dist);
                      // panelty to change lane quickly
                      //if(keep_duration <= 1.)
                      cost += (1.5 / keep_duration);

                    }
                    cout << "cost: " << cost << endl;
                    costs.push_back(cost);
                  }
                  break;
                default:
                  cout <<  "Unknown" << ' ';
              }
            }

            // Find the minimum cost state.
            vector<double>::iterator best_cost = std::min_element(begin(costs),
                                                                  end(costs));
            int best_idx = std::distance(begin(costs), best_cost);

            switch(pos_next_states[best_idx])
            {
              case State::KL:
                {
                  if(ego.front_dist < 30)
                    too_close = true;
                  ego.state = State::KL;

                  lane = ego.lane;
                }
                break;
              case State::LCL:
                {
                  if(ego.left_front_dist < 30)
                    too_close = true;
                  ego.state = State::LCL;
                  ego.lane = ego.lane - 1;

                  lane = ego.lane;
                }
                break;
              case State::LCR:
                {
                  if(ego.right_front_dist < 30)
                    too_close = true;
                  ego.state = State::LCR;
                  ego.lane = ego.lane + 1;

                  lane = ego.lane;
                }
                break;
              default:
                break;
            }

            // print out all the nearby car
            cout << "Goal distance: " << max_s << "\tCurrent distance: " 
              << ego.s << endl;
            cout << "Duration of Keep Lane: " << keep_duration << " seconds\n";

            for(int i=0; i < (int)vec_lane.size(); ++i)
            {
              cout << "Nearby car in Lane " << i << " are \n";
              for(int j=0; j < (int)vec_lane[i].size(); ++j)
              {
                cout << vec_lane[i][j].id << " at s = " 
                  << vec_lane[i][j].s - ego.s
                  << " with speed = " << vec_lane[i][j].v  << endl;
              }
              cout << endl;
            }

            if(too_close)
            {
              ref_vel -= .224*1.5;  // slow down as acc 5 m/s^2
            }
            else if (ref_vel < 49.5)
            {
              ref_vel += .224;  // speed up as acc 5 m/s^2
            }

              

          	// TODO: define a path made up of (x,y) points that the car will visit sequentially every .02 seconds
            // Create a list of widely spaced (x,y) waypoints, evenly spaced at 30m.
            // Later, we will interpolate these waypoints with a spline and fill it in with more points that control speed.
            vector<double> ptsx;
            vector<double> ptsy;

            // reference x, y, yaw state
            // either we will reference the starting points as where the car is or at the previouse paths end point.
            double ref_x = car_x;
            double ref_y = car_y;
            double ref_yaw = deg2rad(car_yaw);
            
            //if previous size is almost empty, use the car as starting reference
            if(prev_size < 2)
            {
              // Use two points that make the path tangent to the car
              double prev_car_x = car_x - cos(car_yaw);
              double prev_car_y = car_y - sin(car_yaw);

              ptsx.push_back(prev_car_x);
              ptsx.push_back(car_x);

              ptsy.push_back(prev_car_y);
              ptsy.push_back(car_y);

            }
            else // use the previous path's end points as starting reference
            {

              // Redefine reference state as previous path end point
              ref_x = previous_path_x[prev_size - 1];
              ref_y = previous_path_y[prev_size - 1];

              double ref_x_prev = previous_path_x[prev_size - 2];
              double ref_y_prev = previous_path_y[prev_size - 2];
              ref_yaw = atan2(ref_y - ref_y_prev, ref_x - ref_x_prev);

              // Use two points that make the path tangent to the previous path's end point
              ptsx.push_back(ref_x_prev);
              ptsx.push_back(ref_x);

              ptsy.push_back(ref_y_prev);
              ptsy.push_back(ref_y);

            }

            // In Frenet add evenly 30m spaced points ahead of the starting reference
            vector<double> next_wp0 = getXY(car_s+30, (2+4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
            vector<double> next_wp1 = getXY(car_s+60, (2+4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);
            vector<double> next_wp2 = getXY(car_s+90, (2+4*lane), map_waypoints_s, map_waypoints_x, map_waypoints_y);

            ptsx.push_back(next_wp0[0]);
            ptsx.push_back(next_wp1[0]);
            ptsx.push_back(next_wp2[0]);

            ptsy.push_back(next_wp0[1]);
            ptsy.push_back(next_wp1[1]);
            ptsy.push_back(next_wp2[1]);

            // shift car reference angle to 0 degree (transfer to car coordinates)
            for(int i=0; i < (int)ptsx.size(); ++i)
            {
              double shift_x = ptsx[i]-ref_x;
              double shift_y = ptsy[i]-ref_y;

              ptsx[i] = (shift_x * cos(0-ref_yaw) - shift_y * sin(0-ref_yaw));
              ptsy[i] = (shift_x * sin(0-ref_yaw) + shift_y * cos(0-ref_yaw));

            }

            // Create a spline
            tk::spline s;

            // set (x,y) points to the spline
            s.set_points(ptsx, ptsy);

            // Define the actual (x,y) points we will ue for the planner
          	vector<double> next_x_vals;
          	vector<double> next_y_vals;

            // Start with all of the previous path points from last time
            for (int i = 0; i < (int)previous_path_x.size(); i++) 
            {
              next_x_vals.push_back(previous_path_x[i]);
              next_y_vals.push_back(previous_path_y[i]);
            }

            // Calculate how to break up spline points so that we travel at our desired reference velocity
            double target_x = 30.0;
            double target_y = s(target_x);
            double target_dist = sqrt( target_x*target_x + target_y*target_y );

            double x_add_on = 0;

            // Fill up the rest of our path planner after filling it with previous points, here we will always output 50 points
            for (int i = 1; i < 50-(int)previous_path_x.size(); i++)
            {

              double N = (target_dist/(.02*ref_vel/2.24));  // 2.24 for transfer mph to meter per seconds
              double x_point = x_add_on + (target_x) / N;
              double y_point = s(x_point);
              

              x_add_on = x_point;

              double x_ref = x_point;
              double y_ref = y_point;

              // rotate back to normal after rotating it earlier (back to world coordinates)
              x_point = (x_ref * cos(ref_yaw) - y_ref * sin(ref_yaw));
              y_point = (x_ref * sin(ref_yaw) + y_ref * cos(ref_yaw));

              x_point += ref_x;
              y_point += ref_y;

              next_x_vals.push_back(x_point);
              next_y_vals.push_back(y_point);
              
            }


          	json msgJson;
          	msgJson["next_x"] = next_x_vals;
          	msgJson["next_y"] = next_y_vals;

          	auto msg = "42[\"control\","+ msgJson.dump()+"]";

          	//this_thread::sleep_for(chrono::milliseconds(1000));
          	ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
          
        }
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }
  });

  // We don't need this since we're not using HTTP but if it's removed the
  // program
  // doesn't compile :-(
  h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data,
                     size_t, size_t) {
    const std::string s = "<h1>Hello world!</h1>";
    if (req.getUrl().valueLength == 1) {
      res->end(s.data(), s.length());
    } else {
      // i guess this should be done more gracefully?
      res->end(nullptr, 0);
    }
  });

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  h.run();
}
