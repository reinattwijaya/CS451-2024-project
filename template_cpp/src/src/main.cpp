#include <chrono>
#include <iostream>
#include <thread>
#include <map>
#include <algorithm>

#include <FIFObroadcast.hpp>
#include "parser.hpp"
#include "hello.h"
#include <signal.h>
#include <PerfectLink.hpp>

using std::cout;
using std::endl;
using std::max;
using std::min;

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
  uint32_t numberOfMessagesSenderNeedToSend = 0; 

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
  int time = 0, broadcastTime = 0;
  uint32_t maxSend = 4000000, maxCount = 1;
  //only increase maxSend if all processes have finished 
  while(true){
    uint32_t counter = 1;
    for(uint32_t i = 1; i <= min(maxSend, numberOfMessagesSenderNeedToSend); i++){
      message += uint32ToString(i);
      if(maxCount <= counter)
          broadCastMessage += "b " + std::to_string(i) + '\n';
      if (i % 8 == 0 || i == numberOfMessagesSenderNeedToSend){
        if(maxCount <= counter)
            outputFile << broadCastMessage;
        Message message_to_send = Message(message, process_host_id, process_host_id, counter, false);
        fifo.broadcast(message_to_send);
        if(maxCount <= counter){
            broadCastMessage = "";
            maxCount++;
        }
        counter++;
        message = "";
      }
      while(i%8 == 0 || i == numberOfMessagesSenderNeedToSend){
        fd_set socks;
        struct timeval t;
        socklen_t len;
        t.tv_sec = time/1000000;
        t.tv_usec = time%1000000;
        FD_ZERO(&socks);
        FD_SET(fifo.udp.getSockfd(), &socks);
        int select_result = select(fifo.udp.getSockfd() + 1, &socks, NULL, NULL, &t);
        //cout << "SELECT RESULT: " << select_result << endl;
        //if it is error or empty, break and go on
        if(select_result <= 0)
            break;
        fifo.process_receive(i);
      }
      if(i == numberOfMessagesSenderNeedToSend/2 || i == numberOfMessagesSenderNeedToSend){
        //cout << "SENDING SPECIAL" << endl;
        struct sockaddr_in receiver_sa;
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
    if(numberOfMessagesSenderNeedToSend > maxSend){
      uint32_t lowest_seq = 2147483647;
      //if you have delivered a lot, broadcast more instead of delivering and vice versa
      for(uint8_t j = 1; j <= hosts.size(); j ++){
        lowest_seq = min(lowest_seq, fifo.lastDelivered[j]);
      }
      maxSend = lowest_seq*8 + 4000000;
      maxSend = maxSend - (maxSend % 8);
    }
    //broadcastTime++;
    //if(broadcastTime %4 == 0){
      //time *= 2;
      //struct timeval tv;
      //tv.tv_sec = time/1000000;
      //tv.tv_usec = time%1000000;
      //setsockopt(fifo.udp.getSockfd(), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof tv);
    //}
  }

  return 0;
}
