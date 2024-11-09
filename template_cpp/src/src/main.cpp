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

  const int BATCH_SIZE = 8;
  const int MSG_PER_SEND = 8;

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
    std::string message = "";
    unsigned long counter = 1;
    unsigned long current_batch = 1;
    for(unsigned long i = 1; i <= numberOfMessagesSenderNeedToSend; i++){
      //assuming we have an array of messages, the abstraction is still in the string
      message += std::to_string(i) + "\n";
      if (i % MSG_PER_SEND == 0 || i == numberOfMessagesSenderNeedToSend){
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
        sendto(sockfd, message_to_send.c_str(), message_to_send.length(), 0, reinterpret_cast<const sockaddr*>(&receiver_sa), sizeof(receiver_sa));
        // while(true){

        //   struct timeval t;
        //   FD_ZERO(&socks);
        //   FD_SET(sockfd, &socks);
        //   t.tv_sec = 1;
        //   t.tv_usec = 0;

        //   int select_result = select(sockfd + 1, &socks, NULL, NULL, &t);
        //   if(select_result == 0){
        //     sendto(sockfd, message_to_send.c_str(), message_to_send.size(), 0, reinterpret_cast<const sockaddr*>(&receiver_sa), sizeof(receiver_sa));
        //   }else if(select_result < 0){
        //     perror("select failed");
        //     break;
        //   }else{
        //     break;
        //   }
        // }

        // n = recvfrom(sockfd, reinterpret_cast<char*>(buffer), 1024, MSG_WAITALL, reinterpret_cast<sockaddr*>(&sender_sa), &len);
        // buffer[n] = '\0';

        if(i % (BATCH_SIZE*MSG_PER_SEND) == 0 || i == numberOfMessagesSenderNeedToSend){
          unsigned long highest_ack = 0;
          struct timeval t;
          FD_ZERO(&socks);
          FD_SET(sockfd, &socks);
          t.tv_sec = 1;
          t.tv_usec = 0;
          while(true){
            cout << highest_ack << ' ' << current_batch << endl;
            int select_result = select(sockfd + 1, &socks, NULL, NULL, &t);
            if(select_result == 0){
              //since the abstraction provides the message string in array, this is still doable
              std::string batch_fixing_message = "";
              for(unsigned long j = highest_ack*MSG_PER_SEND+1; j <= current_batch*MSG_PER_SEND*BATCH_SIZE && j <= numberOfMessagesSenderNeedToSend; j ++){
                batch_fixing_message += std::to_string(j) + "\n";
                if(j % MSG_PER_SEND == 0 || j == numberOfMessagesSenderNeedToSend){
                  unsigned long the_message_id = j/MSG_PER_SEND;
                  std::string outputMessage = std::to_string(the_message_id) + " " + batch_fixing_message;
                  sendto(sockfd, batch_fixing_message.c_str(), batch_fixing_message.length(), 0, reinterpret_cast<const sockaddr*>(&receiver_sa), sizeof(receiver_sa));
                  batch_fixing_message = "";
                }
              }
            }else if(select_result < 0){
              perror("select failed");
              break;
            }else{
              n = recvfrom(sockfd, reinterpret_cast<char*>(buffer), 1024, MSG_WAITALL, reinterpret_cast<sockaddr*>(&sender_sa), &len);
              buffer[n] = '\0';
              highest_ack = std::stoul(buffer);
              if(highest_ack >= current_batch*BATCH_SIZE || highest_ack*8 >= numberOfMessagesSenderNeedToSend){
                break;
              }
            }
          }
          current_batch++;
        }

        message = "";
      }
    }

  }else{
    unsigned long processes_highest_ack[200];
    memset(processes_highest_ack, 0, sizeof(processes_highest_ack));
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
          if(message_id % BATCH_SIZE == 0){
            std::string send_buffer = std::to_string(processes_highest_ack[it->second]);
            sendto(sockfd, send_buffer.c_str(), send_buffer.length(), 0, reinterpret_cast<const sockaddr*>(&sender_sa), len);
          }
          continue;
        }

        std::string message_to_deliver = "";
        for(i++; i < strlen(buffer); i++){
          message_to_deliver += "d " + std::to_string(it->second) + " ";
          for(; i < strlen(buffer) && buffer[i] != '\n'; i++){
            message_to_deliver += buffer[i];
          }
          message_to_deliver += '\n';
        }
        messageMap[std::make_pair(it->second, message_id)] = true;
        outputFile << message_to_deliver;
        if(processes_highest_ack[it->second]+1 == message_id){
          unsigned long cur_message_id = message_id+1;
          while(messageMap.find(std::make_pair(it->second, cur_message_id)) != messageMap.end()){
            cur_message_id++;
          }
          processes_highest_ack[it->second] = cur_message_id-1;
        }
        if(message_id % BATCH_SIZE == 0){
          std::string send_buffer = std::to_string(processes_highest_ack[it->second]);
          sendto(sockfd, send_buffer.c_str(), send_buffer.length(), 0, reinterpret_cast<const sockaddr*>(&sender_sa), len);
        }
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