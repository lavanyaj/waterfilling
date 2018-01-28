#include "weighted_waterfilling.h"
#include <memory>
#include <iostream>
#include <sstream>
#include <algorithm>

WeightedWaterfilling::WeightedWaterfilling(const std::map< link_t, double> & link_capacities) : link_capacities(link_capacities) {};

std::string WeightedWaterfilling::get_str(const link_t& link) {
  std::stringstream ss;
  ss << link.first << "->" << link.second;
  return ss.str();
}

double WeightedWaterfilling::get_sum(const std::vector<double>& summands) {
  // double sum = 0;
  //for (const auto& s : summands) sum += s;
  // kahan summation from wiki
  double sum = 0.0;
  double c = 0.0; // running compensation for lost precision
  for (const auto& s : summands) {
    double y = s - c;
    double t = sum + y;
    c = (t - sum) - y;
    sum = t;
  }
  return sum;
}

void WeightedWaterfillingState::show() {
  std::cout << "waterfilling state in round " << round << std::endl;
  for (const auto& l : active_flows_per_link) {
    std::cout << "Link " << l.first.first 
  	      << "->" << l.first.second << " (";
    if (total_flow_per_link.count(l.first))
      std::cout << "total_flow: " << total_flow_per_link.at(l.first) << " ";          
    if (link_saturated_in_round.count(l.first))
      std::cout << "saturated_in_round: " << link_saturated_in_round.at(l.first) << " ";    
    std::cout << "):  ";

    for (const auto& f : l.second) {
      std::cout << f << " (";
      if (rate_per_flow.count(f)) {
	double weight = flow_to_weight.at(f);
	std::cout << "rate: " << rate_per_flow.at(f) << " (x" << weight <<") ";
      }
      if (flow_saturated_in_round.count(f)) 
	std::cout << "saturated_in_round: " << flow_saturated_in_round.at(f); 
      std::cout << ") ";
    }
    std::cout << std::endl;
  }
}
void WeightedWaterfilling::do_waterfilling(
		const std::map< int, std::vector< link_t > > &
		flow_to_path,
		const std::map< int, double > &
		flow_to_weight,
		std::map< int, double >& rates) {
  //std::unique_ptr<WeightedWaterfillingState> wfs(new WeightedWaterfillingState(flow_to_path));
  WeightedWaterfillingState wfs(flow_to_path, flow_to_weight); 
  //wfs.show();
  while (wfs.unsaturated_flows.size() > 0) {
    do_one_round_of_waterfilling(wfs);
    //    wfs.show();
  }
  //  wfs.show();
  for (auto f : wfs.rate_per_flow) {
    double weight = flow_to_weight.at(f.first);
    rates[f.first] = weight * f.second;
  }
  return;
}

void WeightedWaterfilling::do_one_round_of_waterfilling
(WeightedWaterfillingState& wfs) {
  // the actual waterfilling algorithm
  std::vector<double> fair_share_values;
  std::vector<link_t> fair_share_links;
  // calculate fair shares C/N for all unsaturated links
  // there can be links with no unsat flows
  for (auto& link : wfs.unsaturated_links) {
    if (link_capacities.count(link) == 0 || 
	wfs.total_flow_per_link.count(link) == 0 || 
	wfs.num_unsat_per_link.count(link) == 0) {
      std::cerr << "Link " << link.first << "->" << link.second << " not initialized.\n";
      exit(1);
    }
    double rem_cap = link_capacities.at(link) - wfs.total_flow_per_link.at(link);
    int num_unsat = wfs.num_unsat_per_link.at(link); // number of unsat pseudo flows
    if (num_unsat > 0) {
      double fair_share = rem_cap/num_unsat; // fair share per unsat pseudo flow
      fair_share_values.push_back(fair_share);
      fair_share_links.push_back(link);
    }
  }

  if (fair_share_values.size() == 0) {
    std::cerr << "Didn't find any unsat link carrying an unsat flow.\n";
    exit(1);
  }
  auto min_fair_share_it = std::min_element(fair_share_values.begin(), fair_share_values.end());
  unsigned arg_min = min_fair_share_it - fair_share_values.begin();
  const double min_fair_share_value = *min_fair_share_it;
  const link_t& min_fair_share_link = fair_share_links.at(arg_min);

    // std::cout << "min_fair_share_value is " << min_fair_share_value << " for link "
    // 	      << get_str(min_fair_share_link) << "\n";

    // (re)set rate of all unsat flows to sum of all min_fair_share_values till now
    double increment = min_fair_share_value > 0 ? min_fair_share_value : 0;
    wfs.rate_increments.push_back(increment);
    const double rate_of_an_unsat_flow = get_sum(wfs.rate_increments); // rate of an unsat pseudo flow
    for (auto f : wfs.unsaturated_flows) {
      wfs.rate_per_flow.at(f) = rate_of_an_unsat_flow; // rate of its pseudo flow
    }

    // remove min fair share link and all its unsat flows
    if (wfs.active_flows_per_link.count(min_fair_share_link) == 0) {
      std::cerr << "min_fair_share link " << get_str(min_fair_share_link) << " doens't have active flows.\n";
      exit(1);
    }

    int num_unsat = wfs.num_unsat_per_link.at(min_fair_share_link);
    int backup_num_unsat = 0;
    std::vector<int> unsat_flows_removed;
    for (auto f : wfs.active_flows_per_link.at(min_fair_share_link)) {
      auto flow_it = wfs.unsaturated_flows.find(f);
      if (flow_it != wfs.unsaturated_flows.end()) {
	double weight = wfs.flow_to_weight.at(f);
	backup_num_unsat += weight;
	wfs.flow_saturated_in_round[f] = wfs.round;
	wfs.unsaturated_flows.erase(f);
	unsat_flows_removed.push_back(f);
      }
    }

    if (backup_num_unsat != num_unsat) {
      std::cerr << "min fair share link " << get_str(min_fair_share_link)
		<< " num_unsat " << num_unsat 
		<< " not equal to " << backup_num_unsat
		<< " (book-keeping error?)\n";
      exit(1);
    }

    auto link_it = wfs.unsaturated_links.find(min_fair_share_link);
    if (link_it == wfs.unsaturated_links.end()) {
      std::cerr << "min fair share link " << get_str(min_fair_share_link)
		<< " not in set of unsat links.\n";
      exit(1);
    }

    wfs.unsaturated_links.erase(link_it);
    wfs.link_saturated_in_round[min_fair_share_link] = wfs.round;

    // update total flow and num_unsat on every unsat link
    // to calculate fair share in the next round
    // unsat links that lost all their unsat flows in this round
    // (cuz the flows also passed through min_fair_share link) will
    // get a new total_flow but we won't consider them in our
    // list of fair_share links next round, once num_unsat is 0
    for (auto& l : wfs.unsaturated_links) {
      if (wfs.active_flows_per_link.count(l) == 0) {
	std::cerr << "link " << get_str(l) << " doesn't have active flows.\n";
	exit(1);
      }
      std::vector<double> flow_rates;
      for (auto f : wfs.active_flows_per_link.at(l)) {
	if (wfs.rate_per_flow.count(f) == 0) {
	  std::cerr << "flow " << f << " doesn't have a rate.\n";
	  exit(1);
	}
	double weight = wfs.flow_to_weight.at(f);
	flow_rates.push_back(wfs.rate_per_flow.at(f) * weight); // sum of rates of pseudoflows
      }
      wfs.total_flow_per_link.at(l) = get_sum(flow_rates);
    }

    // re(set) num_unsat flows using link
    wfs.num_unsat_per_link.clear();
    for (auto l : wfs.unsaturated_links) {
      if (wfs.active_flows_per_link.count(l) == 0) {
	std::cerr << "link " << get_str(l) << " doesn't have active flows.\n";
	exit(1);
      }

      num_unsat = 0;
      for (auto f : wfs.active_flows_per_link.at(l)) {
	if (wfs.unsaturated_flows.count(f) > 0) {
	  double weight = wfs.flow_to_weight.at(f);
	  num_unsat += weight;
	}
      }     
      wfs.num_unsat_per_link[l] = num_unsat;
    }

    wfs.round++;
  // at the end wfs will have max-min rates for all (pseudo) flows
};


WeightedWaterfillingState::WeightedWaterfillingState(
 const std::map< int, std::vector< link_t > > & 
 flow_to_path,
 const std::map< int, double> &
 flow_to_weight) : flow_to_weight(flow_to_weight) {
  round = 0;
  //std::cout << "setting up wf state\n";
  for (auto f : flow_to_path) {
    //std::cout << "flow " << f.first << "\n";
    unsaturated_flows.insert(f.first);
    double weight = flow_to_weight.at(f.first);
    rate_per_flow[f.first] = 0;
    for (auto l : f.second) {
      auto link_it = unsaturated_links.insert(l); 
      //first=it to link, second=true if inserted
      auto link = *(link_it.first);
      if (link_it.second) {
      	num_unsat_per_link[link] = 0;
      	total_flow_per_link[link] = 0;
      }
      num_unsat_per_link.at(link) += weight; // number of pseudo flows
      // std::cout << "push flow " << f.first << "onto "  
      // 		<< link.first << "->" << link.second
      // 		<< "\n";
      active_flows_per_link[link].push_back(f.first);      
    }
  }
  return;
}

int main_ignore() {
  std::map< link_t, double> cap;
  cap [std::make_pair(144, 154) ] = 16.0;
  cap [std::make_pair(0, 144) ] = 4;
  cap [std::make_pair(1, 144) ] = 2;
  cap [std::make_pair(2, 144) ] = 10;
  cap [std::make_pair(3, 144) ] = 4;

  WeightedWaterfilling wf(cap);
  std::map< int, std::vector< link_t > > flow_to_path;
  std::map< int, double > flow_to_weight;

  flow_to_path[0].push_back(std::make_pair(0,144));
  flow_to_path[0].push_back(std::make_pair(144, 154));
  flow_to_weight[0] = 5;

  flow_to_path[1].push_back(std::make_pair(1,144));
  flow_to_path[1].push_back(std::make_pair(144, 154));
  flow_to_weight[1] = 8;

  flow_to_path[2].push_back(std::make_pair(2,144));
  flow_to_path[2].push_back(std::make_pair(144, 154));
  flow_to_weight[2] = 1;

  flow_to_path[3].push_back(std::make_pair(3,144));
  flow_to_path[3].push_back(std::make_pair(144, 154));
  flow_to_weight[3] = 2;

  std::map<int, double > rates;
  wf.do_waterfilling(flow_to_path, flow_to_weight, rates);
  for (auto f : rates) {
    std:: cout << "Flow " << f.first << " has rate " << f.second << std::endl;
  }
  return 0;
}
// int main_ignore() {
//   std::map< link_t, double> cap;
//   cap[ std::make_pair(0,144) ] = 1.0;
//   cap[ std::make_pair(144,1) ] = 100.0;
//   //cap[100] = 1.0;
//   Waterfilling wf(cap);
//   std::map< int, std::vector< link_t > > flow_to_path;
//   flow_to_path[1].push_back(std::make_pair(0,144));
//   flow_to_path[1].push_back(std::make_pair(144,1));

//   flow_to_path[2].push_back(std::make_pair(0,144));
//   flow_to_path[3].push_back(std::make_pair(144, 1));
  
//   //flow_to_path[1].push_back(100);
//   std::map<int, double > rates;
//   wf.do_waterfilling(flow_to_path, rates);
//   return 0;
// }
