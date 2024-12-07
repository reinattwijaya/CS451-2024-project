#include <chrono>
#include <iostream>
#include <thread>
#include <map>

#include <FIFObroadcast.hpp>
#include "parser.hpp"
#include "hello.h"
#include <signal.h>
#include <PerfectLink.hpp>

using std::cout;
using std::endl;

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
  unsigned long numberOfMessagesSenderNeedToSend = 0; 

  if (configFile.is_open()) {
    configFile >> numberOfMessagesSenderNeedToSend;
    configFile.close();
  }

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
  FIFOBroadcast fifo(process_host.ip, process_host.port, process_host_id, hosts, outputFile);

  //send messages and periodically wait for messages
  string message = "", broadCastMessage = "";
  bool done = false;
  while(true){
    uint32_t counter = 1;
    for(uint32_t i = 1; i <= numberOfMessagesSenderNeedToSend; i++){
      message += uint32ToString(i);
      if(!done)
          broadCastMessage += "b " + std::to_string(i) + '\n';
      if (i % 8 == 0 || i == numberOfMessagesSenderNeedToSend){
        if(!done)
            outputFile << broadCastMessage;
        Message message_to_send = Message(message, process_host_id, process_host_id, counter, false);
        fifo.broadcast(message_to_send);
        counter++;
        message = "";
        if(!done)
            broadCastMessage = "";
      }
      while(i%8 == 0 || i == numberOfMessagesSenderNeedToSend){
        fd_set socks;
        struct timeval t;
        socklen_t len;
        t.tv_sec = 0;
        t.tv_usec = 0;
        FD_ZERO(&socks);
        FD_SET(fifo.udp.getSockfd(), &socks);
        int select_result = select(fifo.udp.getSockfd() + 1, &socks, NULL, NULL, &t);
        //cout << "SELECT RESULT: " << select_result << endl;
        //if it is error or empty, break and go on
        fifo.process_receive(i);
      }
      if(i == numberOfMessagesSenderNeedToSend/2 || i == numberOfMessagesSenderNeedToSend){
        struct sockaddr_in receiver_sa;
        cout<<"SENDING SPECIAL" << endl;
        //send special message, it has to start with 0
        string special_message = uint8ToString(0);
        for(uint8_t j = 1; j <= hosts.size(); j++){
          special_message += uint32ToString(fifo.lastDelivered[j]);
        }
        for(unsigned long i = 0; i < hosts.size(); i ++){
          if(hosts[i].id == process_host_id)
            continue;
          memset(&receiver_sa, 0, sizeof(receiver_sa));
          receiver_sa.sin_family = AF_INET;
          receiver_sa.sin_addr.s_addr = hosts[i].ip;
          receiver_sa.sin_port = hosts[i].port;
          socklen_t len = sizeof(receiver_sa);
          fifo.udp.send(special_message, reinterpret_cast<sockaddr*>(&receiver_sa));
        }
      }
    }
    done = true;
  }

  return 0;
}
