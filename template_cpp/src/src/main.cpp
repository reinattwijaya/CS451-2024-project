#include <chrono>
#include <iostream>
#include <thread>
#include <map>
#include <unordered_map>

#include <FIFObroadcast.hpp>
#include "parser.hpp"
#include "hello.h"
#include <signal.h>

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

struct pair_hash {
    template <class T1, class T2>
    std::size_t operator () (const std::pair<T1,T2> &p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);

        // Mainly for demonstration purposes, i.e. works but is overly simple
        // In the real world, use sth. like boost.hash_combine
        return h1 ^ h2;  
    }
};

using TheHostMap = std::pair<std::string, unsigned short>;
using TheMessageMap = std::pair<unsigned short, unsigned long>;

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
  unsigned long numberOfMessagesSenderNeedToSend = 0, receiverIdx = 0; 
  std::unordered_map<TheHostMap, unsigned long, pair_hash> hostMap;
  std::unordered_map<TheMessageMap, bool, pair_hash> messageMap;

  if (configFile.is_open()) {
    configFile >> numberOfMessagesSenderNeedToSend >> receiverIdx;
    configFile.close();
  }

  auto hosts = parser.hosts();
  std::string init_ip = "127.0.0.1";
  auto process_host = Parser::Host(0UL, init_ip, static_cast<unsigned short>(0));
  auto receiver_host = Parser::Host(0UL, init_ip, static_cast<unsigned short>(0));
  for (auto &host : hosts) {

    std::pair<std::string, unsigned int> hostKey(host.ipReadable(), host.portReadable());
    hostMap[hostKey] = host.id;

    // get the process id
    if (host.id == parser.id()){
      process_host = host;
    }
    if (host.id == receiverIdx){
      receiver_host = host;
    }
  }

  //input finished, set up UDP 

  PerfectLink perfectLink(process_host.ip, process_host.port, 1000);
  char buffer[1024];

  if (process_host.id != receiverIdx){
    //broadcast the messages to receiverIdx
    struct sockaddr_in receiver_sa;
    memset(&receiver_sa, 0, sizeof(receiver_sa));
    receiver_sa.sin_family = AF_INET;
    receiver_sa.sin_addr.s_addr = receiver_host.ip;
    receiver_sa.sin_port = receiver_host.port;

    fd_set socks;
    std::string message = "";
    unsigned long counter = 1;
    for(unsigned long i = 1; i <= numberOfMessagesSenderNeedToSend; i++){
      message += std::to_string(i) + "\n";
      if (i % 8 == 0 || i == numberOfMessagesSenderNeedToSend){
        std::string outputMessage = "";
        for(unsigned int i = 0; i < message.length(); i ++){
          std::string tempString = "";
          while(message[i] != '\n')
            tempString+=message[i++];
          outputMessage += "b " + tempString + "\n";
        }
        outputFile << outputMessage;
        std::string message_to_send = std::to_string(counter) + " " + message;
        counter++;
        
        perfectLink.send(parseMessage(message_to_send), reinterpret_cast<sockaddr*>(&receiver_sa), buffer, 1024);

        message = "";
      }
    }

  }else{
    //receiver
    while (true) {
      //wait to receive and deliver the messages
      struct sockaddr_in sender_sa;
      Message m = perfectLink.receive(buffer, 1024, reinterpret_cast<sockaddr*>(&sender_sa));
      std::pair<std::string, unsigned short> hostKey(inet_ntoa(sender_sa.sin_addr), ntohs(sender_sa.sin_port));
      auto it = hostMap.find(hostKey);
      if(it != hostMap.end()){
        // make the message to the delivered version, use string since it's easier
        int messageId = m.getSequenceNumber();
        if(messageMap.find(std::make_pair(it->second, messageId)) != messageMap.end()){
          perfectLink.send(parseMessage("0"), reinterpret_cast<sockaddr*>(&sender_sa), buffer, 1024);
          continue;
        }

        messageMap[std::make_pair(it->second, messageId)] = true;
        outputFile << m.createDeliveredMessage(std::to_string(it->second));

      }else{
        perror("host not found");
      }
      perfectLink.send(parseMessage("0"), reinterpret_cast<sockaddr*>(&sender_sa), buffer, 1024);
      // cout << inet_ntoa(sender_sa.sin_addr) << ' ' << sender_sa.sin_port << endl;
    }
  }

  return 0;
}