#include "ideal_simulator.h"
#include <sstream>
#include <string>
#include <iostream>
#include <memory>
#include <cmath>
#include <iomanip> 
IdealSimulator::IdealSimulator(const std::string& flow_filename, 
				 const std::string& out_filename,
			       const std::string& link_filename,
			       double min_bytes_for_priority,
			       double priority_weight):
  flow_filename(flow_filename), out_filename(out_filename), link_filename(link_filename),
  flow_file(flow_filename), out_file(out_filename),
  min_bytes_for_priority_(min_bytes_for_priority), priority_weight_(priority_weight) {

if (not flow_file.is_open()) {
    std::cerr << "Unable to open file " << flow_filename << std::endl;
   exit(1);
}

if (not out_file.is_open()) {
    std::cerr << "Unable to open file " << out_filename << std::endl;
   exit(1);
}

// parse link filename and initialize wf
 std::ifstream link_file(link_filename);
if (not link_file.is_open()) {
    std::cerr << "Unable to open file " << link_filename << std::endl;
   exit(1);
}
std::string line;
std::map<link_t, double > link_capacities;
while (getline (link_file,line)) {
std::string buf1;
std::string buf2;
std::string buf3;
 std::stringstream ss(line);
if (ss >> buf1 and ss >> buf2 and ss >> buf3) {
  int node1 = atoi(buf1.c_str());
  int node2 = atoi(buf2.c_str());
  double cap = atof(buf3.c_str());
 link_capacities[std::make_pair(node1, node2)] = cap;
} else {
std::cerr << "can't parse " << line << " to get link and cap\n";
}
}

std::cout << "set up " << link_capacities.size() << " links.\n";
 wf = std::make_unique<WeightedWaterfilling>(link_capacities);
}


bool IdealSimulator::parse_line(const std::string line) {
std::string buf; // Have a buffer string
std::stringstream ss(line); // Insert the string into a stream

if (ss >> buf) {
    next_flow = atoi(buf.c_str());
} else {
    std::cerr << "couldn't get flow id from " << line << "\n";
    exit(1);
}


if (ss >> buf) {
next_num_bytes = atoi(buf.c_str());
} else {
std::cerr << "couldn't get num_bytes from " << line << "\n";
exit(1);
}

if (ss >> buf) {
next_start = atof(buf.c_str());
} else {
std::cerr << "couldn't get start from " << line << "\n";
exit(1);
}

int prev_node = -1;
while (ss >> buf) {
int node = atoi(buf.c_str());
if (prev_node > 0) {
next_path.push_back(std::make_pair(prev_node, node));
}
prev_node = node;
} 

if (prev_node == -1) {
std::cerr << "couldn't get path from " << line << "\n";
exit(1);
}

std::cout << "parsed line " << line
<< " to get flow_id " << next_flow
<< ", start_time " << next_start
<< ", num_bytes " << next_num_bytes
<< " and path ";
for (auto l : next_path) {
std::cout << l.first << "->" << l.second << " ";
}
std::cout << std::endl;
return true;
}

IdealSimulator::~IdealSimulator() {
if (flow_file.is_open()) {
  flow_file.close();   
}
 if (out_file.is_open()) {
   out_file.close();
 }
}
bool IdealSimulator::get_next_flow() {
  std::cout << "get_next_flow\n";
  // reset
  next_start= -1;
  next_flow = -1;
  next_num_bytes = -1;
  next_path.clear();

  if (not flow_file.is_open()) {
    std::cerr << "Unable to open file " << flow_filename << std::endl;
    exit(1);
  }
  std::string line;
  if (getline (flow_file,line)) {parse_line(line); return true;}       
  return false;
}


void IdealSimulator::add_next_flow_to_active_flows() {
  std::cout << "add next flow to active flows " << next_flow << "\n";
  if (curr_time != next_start) {
    std::cerr << "curr_time not up to date, add next at "
	      << next_start << "\n";
    exit(1);
  }

  flow_start[next_flow] = next_start;
  flow_bytes[next_flow] = next_num_bytes;
  active_flow_paths[next_flow] = next_path;
  active_flow_bytes[next_flow] = next_num_bytes;
  active_flow_weights[next_flow] = 1;
  if (next_num_bytes < min_bytes_for_priority_) {
    active_flow_weights.at(next_flow) = priority_weight_;
  }

  // calculate rates since new flow was added
  wf->do_waterfilling(active_flow_paths, active_flow_weights, rates);

  // reset finish times since rates for flows 
  // have changed (and new flow's added)
  get_new_finish_times();

  if (next_finish < 0) {
    std::cerr << "added flow " << next_flow
	      << " and got new rates but didn't get next_finish.\n";
    exit(1);
  }

  // and next start time
  get_next_flow();
}

void IdealSimulator::get_new_finish_times() {
  // reset old finish times
  next_finish = -1;
  next_flow_to_finish = -1;

  // and get new finish times
  std::map<int, double> finish_times;
  double min_finish_dur = -1;
  double min_finish_flow = -1;
  for (auto f: active_flow_bytes) {
    if (rates.count(f.first) == 0 ||
	rates.at(f.first) <= 0) {
      std::cerr << "invalid rate for flow " << f.first << std::endl;
    }
    if (f.second <= 0) {
      std::cerr << "flow " << f.first << " has invalid bytes " 
		<< f.second << std::endl;
    }
    // rate is in gb/s, size is in bytes
    double rate = rates.at(f.first);
    double dur = (f.second * 8) / (rate * 1e9);
    std::cout << "flow " << f.first << " would finish in "
	      << dur << "\n";
    if (min_finish_dur == -1 or dur < min_finish_dur) {
      min_finish_dur = dur;
      min_finish_flow = f.first;
    }
  }

  if (min_finish_dur > 0) {
    next_finish = curr_time + min_finish_dur;
    next_flow_to_finish = min_finish_flow;
  }
  return;
}

void IdealSimulator::drain_active_flows(double dur) {
  if (dur <= 0) {
    std::cerr << "invalid dur " << dur << std::endl;
    exit(1);
  }

  std::vector<double> new_bytes_values;
  std::vector<int> new_bytes_flows;

  for (auto f: active_flow_bytes) {
    if (rates.count(f.first) == 0 ||
	rates.at(f.first) <= 0) {
      std::cerr << "invalid rate for flow " << f.first << std::endl;
    }
    if (f.second <= 0) {
      std::cerr << "flow " << f.first << " has invalid bytes " 
		<< f.second << std::endl;
    }
    // rate is in gb/s, size is in bytes
    double rate = rates.at(f.first);
    // double dur = (f.second * 8) / (rate * 1e9);
    double bytes_drained = (rate * 1e9 * dur)/8;
    double new_bytes = f.second - bytes_drained;
    if (new_bytes <= 0) {
      std::cerr << "flow " << f.first << " has negative bytes after drain " 
		<< new_bytes << std::endl;
      if (new_bytes < -1) {
	exit(1);
      }
    }
    new_bytes_values.push_back(new_bytes);
    new_bytes_flows.push_back(f.first);
  }

  int n = new_bytes_values.size();
  for (int i = 0; i < n; i++) {
    double bytes = new_bytes_values.at(i);
    int flow = new_bytes_flows.at(i);
    active_flow_bytes.at(flow) = bytes;
  }
  return;
}

// curr_time must be up to date
void IdealSimulator::remove_flows_that_have_finished()
{
  std::vector<int> flows_to_remove;
  for (auto f : active_flow_bytes) {
    if (f.second < 1e-3) {
      flows_to_remove.push_back(f.first);
    }
  }
  
  int num_flows_removed = 0;
  for (auto f : flows_to_remove) {
    double fldur = curr_time - flow_start.at(f) ;
    int src = active_flow_paths.at(f).front().first;
    int dst = active_flow_paths.at(f).back().second;
    out_file << "fid " << f 
		<< std::setprecision(12)
		<< " end_time " << curr_time
		<< " start_time " << flow_start.at(f) 
		<< " fldur " << fldur
		<< std::setprecision(5)
		<< " num_bytes " << flow_bytes.at(f)
		<< " tmp_pkts " 
		<< std::round(flow_bytes.at(f)/1460.0) 
		<< " gid "
		<< src << "-" << dst
		<< "\n";
    num_flows_removed++;
    active_flow_bytes.erase(f);
    active_flow_paths.erase(f);
    active_flow_weights.erase(f);
    flow_end[f] = curr_time;
  }

  std::cout << num_flows_removed 
	    << " flows were removed at " << curr_time << std::endl;

  // calculate rates since we removed some flows
  if (!num_flows_removed) {
    std::cerr << "no flows were removed\n";
    exit(1);
  }

  rates.clear();
  if (active_flow_paths.size() > 0) {
    wf->do_waterfilling(active_flow_paths, active_flow_weights, rates);
  }
  // reset finish times since we removed some flows
  get_new_finish_times();

  return;
}

void IdealSimulator::run() {
  std::cout << "get next flow first time\n";
 get_next_flow();

 // we move curr_time ahead each time we drain flows ..
 if (!active_flow_paths.size()) {
   if (next_flow > 0) {
     std::cout << "add next flow to active flows first time\n";
     // will reset next finish and next start
     curr_time = next_start;
     add_next_flow_to_active_flows();
     std::cout << "next start " << next_flow << " at "
	       << std::setprecision(12)
	       << next_start << "\n";
     std::cout << "next finish " << next_flow_to_finish << " at " 
	       << std::setprecision(12)
	       << next_finish << "\n";
   }
 } 

 bool next_event_is_a_start = true;
 double next_event_time = next_start;

 int num_events = 0;
 while ((next_start > 0 or next_finish > 0) and num_events < 50000) {
   num_events++;
   // see which event to simulate first
   // adding new flow or removing an old flow
   // in either case drain flows until event time first
   if (next_start > 0 and next_finish > 0) {
     if (next_start < next_finish) {
       next_event_is_a_start = true;
       next_event_time = next_start;
       std::cout << "next event start " 
		 << next_flow << " " << next_start << "\n";
     } else {
       next_event_is_a_start = false;
       next_event_time = next_finish;

       std::cout << "next event finish " 
		 << next_flow_to_finish << " "
		 << next_flow_to_finish << "\n";

     }
   } else if (next_start > 0) {
       std::cout << "next event start " 
		 << next_flow << " " << next_start << "\n";

     next_event_is_a_start = true;
     next_event_time = next_start;
   } else {
     next_event_is_a_start = false;
     next_event_time = next_finish;

     std::cout << "next event finish " 
	       << next_flow_to_finish << " "
	       << next_flow_to_finish << "\n";
   }

   
   // this will drain bytes from flows at current rates   
   // if any flows end at next_event_time (will happen
   // if next_event is a finish) then this will remove
   // flows and re-calculate rates and reset next finish 
   // times. 
   // no need to reset next start since once is pending.
   std::cout << "drain_active_flows_until " 
	     << next_event_time  << std::endl;

   double dur = next_event_time - curr_time;
   drain_active_flows(dur);
   curr_time = next_event_time;
   if (next_event_is_a_start) {
     // will reset next start and next finish
     add_next_flow_to_active_flows();

     std::cout << "next start " << next_flow << " at "
	       << std::setprecision(12)
	       << next_start << "\n";
     std::cout << "next finish " << next_flow_to_finish << " at " 
	       << std::setprecision(12)
	       << next_finish << "\n";

   } else {
     // will reset next finish
     remove_flows_that_have_finished();

     std::cout << "next start " << next_flow << " at "
	       << std::setprecision(12)
	       << next_start << "\n";
     std::cout << "next finish " << next_flow_to_finish << " at " 
	       << std::setprecision(12)
	       << next_finish << "\n";

   }
 }

}



// std::vector< link_t > next_flow_path;
// double next_start_time = -1;
// int next_start_flow = -1;
// double next_flow_num_bytes = -1;

// double next_finish_time = -1;
// int next_finish_flow = -1;

  
// do {
//   int active_flows_remain = 0;
//   if (next_finish_time < 0 
// 	and (active_flows_remain = (active_flow_bytes.size() > 0))) {
//     // get earliest flow end time
//     wf.do_waterfilling(active_flow_paths);
//     WaterfillingState wfs;
//     std::vector<double> finish_time_values;
//     std::vector<int> finish_time_flows;
//     for (auto f : active_flow_bytes) {
// 	double bytes_left = f.second;
// 	double rate = wfs.rate.at(f);
// 	if (rate <= 0) {
// 	  exit(1);
// 	}
// 	double dur = bytes_left / rate;
	
// 	double fin_time = curr_time + dur;
// 	finish_time_values.push_back(dur);
// 	finish_time_flows.push_back(f.first);
//     }
      
//     if (finish_time_values.size() == 0) {
// 	exit(1);
//     }
      
//     double min_finish_it = std::min_element(finish_time_values);
//     int arg_min_finish = min_finish_it - finish_time_values.begin();
//     int flow_min_finish = finish_time_flows.at(arg_min_finish);      
//     next_finish_time = *min_finish_it;
//   }
    
//   int pending_flows = 0;
//   if (next_start_time < 0 and (pending_flows = getline (myfile,line))) {
//     int parsed = parse_line(line, flow_id, path, start_time, num_bytes);
//     if (!parsed) {
// 	std::cerr << "Unable to parse line to get flow info " << line << std::endl;
// 	exit(1);
//     }      
//   }
    
// } while (next_start_time > 0 or next_finish_time > 0)


int main(int argc, char** argv) {
  if (argc != 6) {
    std::cerr << "Expected 5 arguments to binary- flow, out and link file, min bytes for priority, priority weight\n";
    exit(1);
  }
  std::cout << "Got " << argv[1] << ", " << argv[2] << ", " << argv[3]
	    << atof(argv[4]) << ", " << atof(argv[5])
	    << std::endl;
  //IdealSimulator sim("flow_file.txt","out_file.txt","link_file.txt");
  //IdealSimulator sim("all-topo0-80pc.txt","fcts-96-topo0-80pc.txt","l1-96.txt");
  IdealSimulator sim(argv[1], argv[2], argv[3], atof(argv[4]), atof(argv[5]));
  sim.run();
}
