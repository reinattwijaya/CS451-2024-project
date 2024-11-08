#include <chrono>
#include <iostream>
#include <thread>
#include <string>
#include <map>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

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

int main(int argc, char **argv) {
  signal(SIGTERM, stop);
  signal(SIGINT, stop);

  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
  bool requireConfig = false;

  Parser parser(argc, argv);
  parser.parse();

  hello();
  cout << endl;

  std::ifstream configFile(parser.configPath());
  outputFile.open(parser.outputPath());
  unsigned long numberOfMessagesSenderNeedToSend = 0, receiverIdx = 0; 
  std::map<std::pair<std::string, unsigned short>, unsigned long> hostMap;
  std::map<std::pair<unsigned short, unsigned long>, bool> messageMap;

  if (configFile.is_open()) {
    configFile >> numberOfMessagesSenderNeedToSend >> receiverIdx;
    configFile.close();
  }

  cout << "Number of messages sender need to send: " << numberOfMessagesSenderNeedToSend << endl;
  cout << "Receiver index: " << receiverIdx << endl;

  cout << "My PID: " << getpid() << "\n";
  cout << "From a new terminal type `kill -SIGINT " << getpid() << "` or `kill -SIGTERM "
            << getpid() << "` to stop processing packets\n\n";

  cout << "My ID: " << parser.id() << "\n\n";

  cout << "List of resolved hosts is:\n";
  cout << "==========================\n";
  auto hosts = parser.hosts();
  std::string init_ip = "127.0.0.1";
  auto process_host = Parser::Host(0UL, init_ip, static_cast<unsigned short>(0));
  auto receiver_host = Parser::Host(0UL, init_ip, static_cast<unsigned short>(0));
  for (auto &host : hosts) {
    cout << host.id << "\n";
    cout << "Human-readable IP: " << host.ipReadable() << "\n";
    cout << "Machine-readable IP: " << host.ip << "\n";
    cout << "Human-readbale Port: " << host.portReadable() << "\n";
    cout << "Machine-readbale Port: " << host.port << "\n";
    cout << "\n";

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
  cout << process_host.id << "\n";
  cout << "\n";

  cout << "Path to output:\n";
  cout << "===============\n";
  cout << parser.outputPath() << "\n\n";

  cout << "Path to config:\n";
  cout << "===============\n";
  cout << parser.configPath() << "\n\n";

  cout << "Doing some initialization...\n\n";

  cout << "Broadcasting and delivering messages...\n\n";

  //input finished, set up UDP 

  struct sockaddr_in process_sa, sender_sa;
  ssize_t n;
  socklen_t len;
  len = sizeof(sender_sa);
  char buffer[1024];

  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE); 
  }
  memset(&process_sa, 0, sizeof(process_sa));

  process_sa.sin_family = AF_INET;
  process_sa.sin_addr.s_addr = process_host.ip;
  process_sa.sin_port = process_host.port;

  if (bind(sockfd, reinterpret_cast<const sockaddr*>(&process_sa), sizeof(process_sa)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (process_host.id != receiverIdx){
    //broadcast the messages to receiverIdx
    struct sockaddr_in receiver_sa;
    memset(&receiver_sa, 0, sizeof(receiver_sa));
    receiver_sa.sin_family = AF_INET;
    receiver_sa.sin_addr.s_addr = receiver_host.ip;
    receiver_sa.sin_port = receiver_host.port;
    fd_set socks;
    //not sure if this is needed as an abstraction
    std::map<std::string, bool> messagesSent;

    while(true){

      std::string message = "";
      unsigned long counter = 1;

      for(unsigned long i = 1; i <= numberOfMessagesSenderNeedToSend; i++){
        if(messagesSent[std::to_string(counter)])
          continue;
        message += "b " + std::to_string(i) + "\n";
        if (i % 8 == 0){
          outputFile << message;
          std::string message_to_send = std::to_string(counter) + " " + message;
          counter++;
          sendto(sockfd, message_to_send.c_str(), message_to_send.size(), 0, reinterpret_cast<const sockaddr*>(&receiver_sa), sizeof(receiver_sa));
          message = "";
        }
      }
      if(message != ""){
        std::string message_to_send = std::to_string(counter) + " " + message;
        counter++;
        sendto(sockfd, message_to_send.c_str(), message_to_send.size(), 0, reinterpret_cast<const sockaddr*>(&receiver_sa), sizeof(receiver_sa));
      }

      struct timeval t;
      FD_ZERO(&socks);
      FD_SET(sockfd, &socks);
      t.tv_sec = 0;
      t.tv_usec = 500000;

      //now we get all the ACK in the buffer
      while(true){
        int select_result = select(sockfd + 1, &socks, NULL, NULL, &t);
        cout << select_result << endl;
        if (select_result == 0){
          break;
        }else if(select_result < 0){
          perror("select failed");
          exit(EXIT_FAILURE);
        }
        n = recvfrom(sockfd, reinterpret_cast<char*>(buffer), 1024, MSG_WAITALL, reinterpret_cast<sockaddr*>(&sender_sa), &len);
        std::string ack = buffer;
        cout << ack << endl;
        messagesSent[ack] = true;
      }
    }

  }else{
    //receiver
    while (true) {
      //wait to receive and deliver the messages

      // std::this_thread::sleep_for(std::chrono::hours(1));

      n = recvfrom(sockfd, reinterpret_cast<char*>(buffer), 1024, MSG_WAITALL, reinterpret_cast<sockaddr*>(&sender_sa), &len);
      buffer[n] = '\0';
      std::pair<std::string, unsigned short> hostKey(inet_ntoa(sender_sa.sin_addr), ntohs(sender_sa.sin_port));
      auto it = hostMap.find(hostKey);
      if(it != hostMap.end()){
        // make the message to the delivered version, use string since it's easier
        unsigned long i = 0;
        std::string message_id_str = "";
        while(buffer[i] != ' '){
          message_id_str += buffer[i];
          i++;
        }
        unsigned long message_id = std::stoul(message_id_str);
        if(messageMap.find(std::make_pair(it->second, message_id)) != messageMap.end()){
          // sendto(sockfd, "ACK", sizeof("ACK"), 0, reinterpret_cast<const sockaddr*>(&sender_sa), len);
          continue;
        }

        std::string message_to_deliver = "";
        for(; i < strlen(buffer); i++){
          if (buffer[i] == 'b'){
            message_to_deliver += "d " + std::to_string(it->second) + " ";
            for(i+=2; i < strlen(buffer) && buffer[i] != '\n'; i++){
              message_to_deliver += buffer[i];
            }
            message_to_deliver += '\n';
          }
        }
        messageMap[std::make_pair(it->second, message_id)] = true;
        outputFile << message_to_deliver;
        cout << message_id_str << endl;
        sendto(sockfd, message_id_str.c_str(), sizeof(message_id_str), 0, reinterpret_cast<const sockaddr*>(&sender_sa), len);

      }else{
        perror("host not found");
      }
      // cout << inet_ntoa(sender_sa.sin_addr) << ' ' << sender_sa.sin_port << endl;
    }
  }

  return 0;
}



/*
current algorithm: 
SENDER
1. loop over all the messages need to be sent
2. every 8 messages, keep sending the messages until you get the ACK from the receiver
3. if you get the ACK, then loop back up

RECEIVER
1. wait for the message from the sender
2. get the host id from the ip and port information
3. check the message id, if it is already received, then ignore
4. if it is not received, then deliver the message -> loop through all the messages and put it into the string
5. loop back up and wait for the next message

NEW algorithm:
SENDER
1. loop over all the messages need to be sent
2. keep sending x messages until the end of the message (x needs to be experimented on)
3. at the end of the loop, recvmmsg the ACK you get from the receiver, decode it and acknowledge it in the map
4. loop back up and send message that hasn't receive the ACK

RECEIVER
1. recvmmsg the messages from the sender
2. make a thread for the recvmmsg that you have received to deliver the message (distinction between receiving message and delivering)
3. deliver the message that hasn't been delivered based on host and message id

*/