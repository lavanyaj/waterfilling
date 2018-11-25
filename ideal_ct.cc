#include "ideal_ct.h"
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
			       double priority_weight,
			       double max_sim_time):
  flow_filename(flow_filename), out_filename(out_filename), link_filename(link_filename),
  flow_file(flow_filename), out_file(out_filename),
  min_bytes_for_priority_(min_bytes_for_priority), priority_weight_(priority_weight),
  max_sim_time_(max_sim_time) {

if (not flow_file.is_open()) {
    std::cerr << "Unable to open file " << flow_filename << std::endl;
   exit(1);
}

if (not out_file.is_open()) {
    std::cerr << "Unable to open file " << out_filename << std::endl;
   exit(1);
}

if (max_sim_time_ <= 0) {
  std::cerr << "invalid max_sim_time " << max_sim_time_ << std::endl;
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


bool IdealSimulator::parse_line(const std::string line, double& start_or_end, int& flow, double& num_bytes, std::vector< link_t >& path) {

std::string buf; // Have a buffer string
std::stringstream ss(line); // Insert the string into a stream

if (ss >> buf) {
    flow = atoi(buf.c_str());
} else {
    std::cerr << "couldn't get flow id from " << line << "\n";
    exit(1);
}


 if (ss >> buf) {
  num_bytes = atol(buf.c_str());
 } else {
std::cerr << "couldn't get num_bytes from " << line << "\n";
exit(1);
}

 if (num_bytes > 0) {
   // adding a flow, will update next_start
   if (ss >> buf) {
     start_or_end = atof(buf.c_str());
   } else {
     std::cerr << "couldn't get start from " << line << "\n";
     exit(1);
   }

   int prev_node = -1;
   while (ss >> buf) {
     int node = atoi(buf.c_str());
     if (prev_node >= 0) {
       path.push_back(std::make_pair(prev_node, node));
     }
     prev_node = node;
   } 

   if (prev_node == -1) {
     std::cerr << "couldn't get path from " << line << "\n";
     exit(1);
   }

   std::cout << "parsed line " << line
	     << " to get flow_id " << flow
	     << ", start_time " << start_or_end
	     << ", num_bytes " << num_bytes
	     << " and path ";
   for (auto l : path) {
     std::cout << l.first << "->" << l.second << " ";
   }
   std::cout << std::endl;
 } else {
   // removing a flow, will update next_finish
   if (ss >> buf) {     
     start_or_end = atof(buf.c_str());
   } else {
     std::cerr << "couldn't get finish from " << line << "\n";
     exit(1);
   }
 }

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
  // how to read two flows at a time?
  // if peek_parsed = False, then read line and read line again
  // otherwise copy peek vars to next and read line to peek
  // and how to check if peek_start or peek_end equals next_start .. do-able
  std::cout << "get_next_flow\n";


  if (not flow_file.is_open()) {
    std::cerr << "Unable to open file " << flow_filename << std::endl;
    exit(1);
  }

  std::string line;    
  if (peek_parsed) {
    // we'll copy peek to next and read line into peek
    next_start_or_end= peek_start_or_end;
    next_flow = peek_flow;
    next_num_bytes = peek_num_bytes;
    next_path = peek_path;    
    peek_parsed = false;

    // reset
    peek_start_or_end= -1;
    peek_flow = -1;
    peek_num_bytes = -1;
    peek_path.clear();

    if (getline (flow_file,line)) {
      peek_parsed=parse_line(line,peek_start_or_end,\
			     peek_flow,\
			     peek_num_bytes,\
			     peek_path); 
    }
    return true;
  } else {
    // we'll read one line into next, next line .. peek
    peek_parsed = false;
    parsed = false;

    // reset
    next_start_or_end= -1;
    next_flow = -1;
    next_num_bytes = -1;
    next_path.clear();

    // reset
    peek_start_or_end= -1;
    peek_flow = -1;
    peek_num_bytes = -1;
    peek_path.clear();

    if (getline (flow_file,line)) {
      parsed=parse_line(line,next_start_or_end,	\
			next_flow,	       \
			next_num_bytes,	       \
			next_path); 
      line.clear();
      if (parsed and getline (flow_file,line)) {
	peek_parsed=parse_line\
	  (line,peek_start_or_end,	       \
	   peek_flow,			       \
	   peek_num_bytes,		       \
	   peek_path);           
      }
    }
    return parsed;
  }
}


void IdealSimulator::log_rates() {
     std::cout << "at time " << curr_time << " " 
	       << active_flow_paths.size() << " active flows total \n";
     int af_uplink_0 = 0;
     for (auto f : rates) {
       int src = active_flow_paths.at(f.first).front().first;
       std::cout << "RATE_CHANGE " 
		 << f.first << " "
		 << curr_time <<  " "
		 << f.second << "\n";	 
       // std::cout << "at time " << curr_time << " rate of flow " 
       // 		 << f.first << " is " << f.second 
       // 		 << " bytes " << active_flow_bytes.at(f.first) 
       // 		 << " out of " << flow_bytes.at(f.first)
       // 		 << " gid " << src
       // 		 << "-" << active_flow_paths.at(f.first).back().second
       // 		 << "\n";
       if (src == 0) af_uplink_0++;
     }
     std::cout << "at time " << curr_time << " " 
	       << af_uplink_0 << " active flows on 0->\n";

}
void IdealSimulator::add_next_flow_to_active_flows() {
  std::cout << "add next flow to active flows " << next_flow << "\n";
  if (curr_time != next_start_or_end) {
    std::cerr << "curr_time not up to date, add next at "
	      << next_start_or_end << "\n";
    exit(1);
  }

  flow_start[next_flow] = next_start_or_end;
  flow_bytes[next_flow] = next_num_bytes;
  active_flow_paths[next_flow] = next_path;
  active_flow_bytes[next_flow] = next_num_bytes;
  active_flow_weights[next_flow] = 1;
  if (next_num_bytes < min_bytes_for_priority_) {
    active_flow_weights.at(next_flow) = priority_weight_;
  }

  if (!peek_parsed \
      or peek_start_or_end > next_start_or_end) {
    remove_flows_that_have_finished();
  
    if (next_finish < 0) {
      std::cerr << "added flow " << next_flow
		<< " and got new rates but didn't get next_finish.\n";
      exit(1);    
    }
  }
  // and next start or end time
  get_next_flow();
}

void IdealSimulator::get_new_finish_times() {
  //  std::cout << "get new finish times\n";
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
    if (f.second < -1e-6) {
      std::cerr << "flow " << f.first << " has invalid bytes " 
		<< f.second << std::endl;
    }
    // rate is in gb/s, size is in bytes
    double rate = rates.at(f.first);
    double dur = (f.second * 8) / (rate * 1e9);

    // std::cout << "curr_time " << curr_time << " "
    // 	      << "flow " << f.first << " has rate " << rates.at(f.first)
    // 	      << " and will finish in " << dur << "\n";

    //std::cout << "flow " << f.first << " would finish in "
    //	      << dur << "\n";
    if (min_finish_dur == -1 or dur < min_finish_dur) {
      min_finish_dur = dur;
      min_finish_flow = f.first;
    }
  }

  if (min_finish_dur > 0) {
    next_finish = curr_time + min_finish_dur;
    next_flow_to_finish = min_finish_flow;
  }
  // std::cout << "new finish times: curr_time " << curr_time
  // 	    << " next finish time is " << next_finish
  // 	    << " for flow " << next_flow_to_finish << "\n";
  return;
}

void IdealSimulator::end_next_flow_in_active_flows() {
  //  std::cout << "end next flow in active flows " << next_flow << "\n";
  if (curr_time != next_start_or_end) {
    std::cerr << "curr_time not up to date, end next at "
	      << next_start_or_end << "\n";
    exit(1);
  }

  //  flow_start[next_flow] = next_start;
  //flow_bytes[next_flow] = next_num_bytes;
  //active_flow_paths[next_flow] = next_path;
  active_flow_bytes[next_flow] = 0; //next_num_bytes;
  //active_flow_weights[next_flow] = 1;
  if (!peek_parsed \
      or peek_start_or_end > next_start_or_end) {
    remove_flows_that_have_finished();
  }
  // and next start or end time
  get_next_flow();

}

void IdealSimulator::drain_active_flows(double dur) {
  if (dur <= 0) {
    std::cerr << "possibly invalid dur " << dur 
	      << " at curr_time " << curr_time
	      << " exit if < -1us"
	      << std::endl;
    if (dur < -1e-6) exit(1);
    return;
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
  
  std::vector<int> flows_removed;
  std::stringstream ss;
  int num_flows_removed = 0;
  for (auto f : flows_to_remove) {
    double fldur = curr_time - flow_start.at(f) ;
    int src = active_flow_paths.at(f).front().first;
    int dst = active_flow_paths.at(f).back().second;
    // input file has bytes on the wire
    double payload_bytes = (flow_bytes.at(f)/1500.0)*1460.0;
    flows_removed.push_back(f);
    ss << f << " ";
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

  // std::cout << num_flows_removed 
  //  	    << " flows were removed at " << curr_time << std::endl;

  std::cout << "DONE " << num_flows_removed << " " << ss.str() << std::endl;
  for (auto f: flows_removed) {
    std::cout << "RATE_CHANGE " 
	      << f << " "
	      << curr_time <<  " "
	      << 0 << "\n";
  }
  // We call this function after removing/ adding flows too
  // calculate rates since we removed some flows
  // if (!num_flows_removed) {
  //   std::cerr << "no flows were removed\n";
  //   exit(1);
  // }

  rates.clear();
  if (active_flow_paths.size() > 0) {
    wf->do_waterfilling(active_flow_paths, active_flow_weights, rates);
  }
  // reset finish times since we removed some flows
  get_new_finish_times();

  return;
}

void IdealSimulator::run() {
  //  std::cout << "get next flow first time\n";
 get_next_flow();

 // we get next event from file : it's a start flow or an end flow at some time
 // we also have for the current set of flows, a next finish time
 // we find which event comes first start flow, end flow or finish flow.
 // say it's start flow then drain flows from now until start time, add flow, recompute finish times
 // and it's end flow then drain flows from now until end time, remove flow, recompute finish times
 // and if it's finish flows then ..
 // what if many events at the same time?? cal it a multi-event
 // then drain flows from now until multi-event, add and remove flows, recompute finish times

 // we move curr_time ahead each time we drain flows ..
 if (!active_flow_paths.size()) {
   
   std::cout << "RATE_CHANGE fid time(s) rate\n";

   if (next_flow > 0 and next_start_or_end > 0) {
     std::cout << "add next flow to active flows first time\n";
     // will reset next finish and next start
     curr_time = next_start_or_end;
     add_next_flow_to_active_flows();
     log_rates();
     std::cout << "next start " << next_flow << " at "
	       << std::setprecision(12)
	       << next_start_or_end << "\n";
     std::cout << "next finish " << next_flow_to_finish << " at " 
	       << std::setprecision(12)
	       << next_finish << "\n";
   }
 } 

 Event next_event = Event::nd; 
 double next_event_time = -1;

 int num_events = 0;
 while ((next_start_or_end > 0 or next_finish > 0) and num_events < 500000) {
   num_events++;
   
   // assert next_event_time -1, next_event = nd

   if (next_start_or_end > 0) { 
     next_event_time = next_start_or_end; 
     if (next_num_bytes > 0)
       next_event = Event::start;
     else next_event = Event::end;
   }
   if (next_finish > 0 and (next_event_time < 0 or next_finish < next_event_time)) {
     next_event_time = next_finish;
     next_event = Event::finish;
   }

   if (next_event == Event::end) {
       std::cout << "next event end " 
		 << next_start_or_end << " "
		 << next_flow << "\n";
   } else if (next_event == Event::finish) {
       std::cout << "next event finish " 
		 << next_finish << " "
		 << next_flow_to_finish << "\n";
   } else if (next_event == Event::start) {
       std::cout << "next event start " 
		 << next_start_or_end << " " 
		 << next_flow << "\n";
   } else {
       std::cerr << "next event nd, next start_or_end " 
		 << next_start_or_end << "(flow " << next_flow << ")"
		 << " next_finish" 
		 << next_finish << "(flow " << next_flow_to_finish << ")\n";
       exit(1);
   }
   
   
   double dur = next_event_time - curr_time;
   // will drain flows at latest rates only if next
   // event is after curr_time, we make sure that we
   // do have latest rates at this point (see below)
   if (dur > 0) {
     // drain flows only if next_event_time has
     // changed from curr_time?
     std::cout << "drain_active_flows_until " 
	       << next_event_time  << std::endl;

     drain_active_flows(dur);
     curr_time = next_event_time;
   }

   bool should_log_rates = false;
   if (!peek_parsed or peek_start_or_end > next_start_or_end) should_log_rates = true;

   // if peek_start_or_end is different from next_start_or_end, 
   // add_..(), end_..() will re-calculate rates and reset next_finish, next_start

   if (next_event == Event::start) {
     // will reset next start and next finish
     add_next_flow_to_active_flows();
     if (should_log_rates) log_rates();
   } else if (next_event == Event::finish) {
     // will reset next finish
     remove_flows_that_have_finished();
     log_rates();
   } else if (next_event == Event::end) {
     end_next_flow_in_active_flows();
     if (should_log_rates) log_rates();
   } else {
     std::cout << "no more events.\n";
   }

   if (next_event_time >= max_sim_time_) {
     std::cout << "next_event_time " << next_event_time 
	       << " exceeds max_sim_time_ " << max_sim_time_
	       << std::endl;
     break;
   }

   next_event = Event::nd; 
   next_event_time = -1;
 }
}



int main(int argc, char** argv) {
  if (argc != 7) {
    std::cerr << "Expected 5 arguments to binary- flow, out and link file, min bytes for priority, priority weight, max_sim_time \n";
    exit(1);
  }
  std::cout << "Got " << argv[1] << ", " << argv[2] << ", " << argv[3]
	    << atof(argv[4]) << ", " << atof(argv[5]) << ", " << atof(argv[6])
	    << std::endl;
  //IdealSimulator sim("flow_file.txt","out_file.txt","link_file.txt");
  //IdealSimulator sim("all-topo0-80pc.txt","fcts-96-topo0-80pc.txt","l1-96.txt");
  IdealSimulator sim(argv[1], argv[2], argv[3], atof(argv[4]), atof(argv[5]), atof(argv[6]));
  sim.run();
  return 0;
}
