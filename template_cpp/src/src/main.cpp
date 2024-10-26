#include <chrono>
#include <iostream>
#include <thread>
#include <string>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "parser.hpp"
#include "hello.h"
#include <signal.h>

using std::cout;
using std::endl;

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  // immediately stop network packet processing
  cout << "Immediately stopping network packet processing.\n";

  // write/flush output file if necessary
  cout << "Writing output.\n";

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
  unsigned long numberOfMessagesSenderNeedToSend = 0, receiverIdx = 0; 
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
  auto process_host = Parser::Host();
  auto receiver_host = Parser::Host();
  for (auto &host : hosts) {
    cout << host.id << "\n";
    cout << "Human-readable IP: " << host.ipReadable() << "\n";
    cout << "Machine-readable IP: " << host.ip << "\n";
    cout << "Human-readbale Port: " << host.portReadable() << "\n";
    cout << "Machine-readbale Port: " << host.port << "\n";
    cout << "\n";
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

  const std::string test = "test";
  struct sockaddr_in process_sa;
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

    sendto(sockfd, test.c_str(), test.size(), 0, reinterpret_cast<const sockaddr*>(&receiver_sa), sizeof(receiver_sa));
    cout << "Hello message sent to receiver" << endl;

  }else{
    //wait to receive and deliver the messages
    struct sockaddr_in sender_sa;

    ssize_t n;
    socklen_t len;
    len = sizeof(sender_sa);
    char buffer[1024];

    n = recvfrom(sockfd, reinterpret_cast<char*>(buffer), 1024, MSG_WAITALL, reinterpret_cast<sockaddr*>(&sender_sa), &len);
    buffer[n] = '\0';
    cout << "CLIENT: " << buffer << endl;
    cout << inet_ntoa(sender_sa.sin_addr) << ' ' << htons(sender_sa.sin_port) << endl;

  }

  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  while (true) {
    std::this_thread::sleep_for(std::chrono::hours(1));
  }

  return 0;
}
