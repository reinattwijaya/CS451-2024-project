#include <chrono>
#include <iostream>
#include <thread>
#include <map>
#include <algorithm>

#include <LatticeAgreement.hpp>
#include "parser.hpp"
#include "hello.h"
#include <signal.h>
#include <PerfectLink.hpp>

using namespace std;

//a bit dangerous, but we need to write to the output file from the signal handler
std::ofstream outputFile;

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  // immediately stop network packet processing
  cout << "Immediately stopping network packet processing.\n";

  // write/flush output file if necessary
  cout << "Writing output.\n";

  outputFile.flush();
  outputFile.close();

  // exit directly from signal handler
  exit(0);
}

int main(int argc, char **argv) {
  signal(SIGTERM, stop);
  signal(SIGINT, stop);

  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
  bool requireConfig = false;

  Parser parser(argc, argv);
  parser.parse();

  std::ifstream configFile(parser.configPath());
  outputFile.open(parser.outputPath());
  uint32_t numberOfProposals = 0, maxProposalSize = 0, maxDistinctElements = 0; 
  vector<vector<uint32_t>> proposals;
  if (configFile.is_open()) {
    configFile >> numberOfProposals >> maxProposalSize >> maxDistinctElements;
    string line;
    getline(configFile, line);
    for(uint32_t i = 0; i < numberOfProposals; i ++){
      vector<uint32_t> proposal;
      getline(configFile, line);
      stringstream line_stream(line);
      uint32_t num;
      while(line_stream >> num)
        proposal.push_back(num);
      sort(proposal.begin(), proposal.end());
      proposals.push_back(proposal);
    }
    configFile.close();
  }

  // cout << numberOfProposals << " " << maxProposalSize << " " << maxDistinctElements << endl;
  // for(uint32_t i = 0; i < numberOfProposals; i ++){
  //   for(uint32_t j = 0; j < proposals[i].size(); j ++)
  //     cout << proposals[i][j] << " ";
  //   cout << endl;
  // }

  auto hosts = parser.hosts();
  string init_ip = "127.0.0.1";
  auto process_host = Parser::Host(0UL, init_ip, static_cast<unsigned short>(0));
  for (auto &host : hosts) {
    if (host.id == parser.id()){
      process_host = host;
      break;
    }
  }

  //input finished, set up UDP

  uint8_t process_host_id = static_cast<uint8_t>(process_host.id);
  LatticeAgreement la(process_host.ip, process_host.port, process_host_id, hosts, proposals, outputFile);

  for(uint32_t i = 0; i < numberOfProposals; i ++){
    la.propose();
    la.receive(maxDistinctElements);
  }
  while(true){
    la.receive(maxDistinctElements);
  }

  return 0;
}
