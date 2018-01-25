#include <map>
#include <vector>
#include <set>
#include <utility> // std::pair
typedef std::pair<int, int> link_t;

//typedef int link_t;

class Waterfilling;

// we only care about which links are used, order doesn't matter
class WaterfillingState {
  friend Waterfilling;
 protected:
  void show();
  int round;
  std::set< link_t > unsaturated_links; 
  std::set< int > unsaturated_flows;
  std::map< link_t, int > num_unsat_per_link;
  std::map< link_t, double > total_flow_per_link;
  std::map< link_t, std::vector< int > > active_flows_per_link;
  std::map< int, double > rate_per_flow;
  
  std::map< int, int > flow_saturated_in_round;
  std::map< link_t, int > link_saturated_in_round;

  std::vector<double> rate_increments;
 public:
WaterfillingState(const std::map<int, std::vector< link_t > > & flow_to_path);
};

class Waterfilling {
 protected:
  std::map< link_t, double> link_capacities;
  std::map<int, std::vector< link_t > > flow_to_path;

 public:
  Waterfilling(const std::map< link_t, double>& link_capacities);
  static std::string get_str(const link_t & link);
  static double get_sum(const std::vector<double> & summands);
  void do_one_round_of_waterfilling(WaterfillingState& wfs);
  void do_waterfilling(const std::map<int, std::vector< link_t > >& flow_to_path, 
                            std::map<int, double >& rates);
};
