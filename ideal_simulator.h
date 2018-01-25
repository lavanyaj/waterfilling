#include "weighted_waterfilling.h"
#include <memory>
#include <string>
#include <sstream>
#include <fstream>

class IdealSimulator {
 protected:
  std::string flow_filename;
  std::string out_filename;
  std::string link_filename;

  double min_bytes_for_priority_;
  double priority_weight_;

  //std::map<int, double> flow_arrivals;
  //std::map<int, std::vector< link_t > > flow_paths;

  std::map<int, std::vector< link_t > > active_flow_paths;
  std::map<int, double > active_flow_bytes;
  std::map<int, double > active_flow_weights;

  std::map<int, double> flow_start;
  std::map<int, double> flow_end;
  std::map<int, double> flow_bytes;

  std::unique_ptr<WeightedWaterfilling> wf;

  std::ifstream flow_file;
  std::ofstream out_file;
  double curr_time = -1;

  std::map<int, double> rates;

  double next_finish = -1;
  double next_flow_to_finish = -1;

  double next_start = -1;
  int next_flow = -1;
  std::vector< link_t > next_path;
  double next_num_bytes = -1;
  // read flow_file and populate next_start, next_flow, .. 
  // with details of next flow to start
  bool get_next_flow(); 
  bool parse_line(const std::string line);

  void add_next_flow_to_active_flows();
  void drain_active_flows(double dur);
  void remove_flows_that_have_finished();
  void get_new_finish_times();

 public:
  IdealSimulator(const std::string& flow_filename, 
		 const std::string& out_filename,
		 const std::string& link_filename,
		 double min_bytes_for_priority,
		 double priority_weight);
  ~IdealSimulator();
  void run();
};
