#pragma once
#include <Message.hpp>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <cstring>

class UDP{
    private:
        int sockfd, sender_sockfd;
        struct sockaddr_in process_sa;
        char buffer[1024];
    public:
        //create a udp socket and bind it to the given ip and port
        UDP(in_addr_t ip, in_port_t port){
            sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd < 0) {
                perror("socket creation failed");
                exit(EXIT_FAILURE); 
            }
            memset(&process_sa, 0, sizeof(process_sa));

            process_sa.sin_family = AF_INET;
            process_sa.sin_addr.s_addr = ip;
            process_sa.sin_port = port;

            if (bind(sockfd, reinterpret_cast<const sockaddr*>(&process_sa), sizeof(process_sa)) < 0) {
                perror("bind failed");
                exit(EXIT_FAILURE);
            }
            sender_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if (sender_sockfd < 0) {
                perror("sender socket creation failed");
                exit(EXIT_FAILURE); 
            }
        }
        //the sender uses a random port assigned by the OS
        void send(string message, const sockaddr* receiver_sa){
            //std::cout << message.createDeliveredMessage() << std::endl;
            //std::cout << "SEQ: " << static_cast<unsigned int>(message.getSequenceNumber()) << std::endl;
            ssize_t bytes_sent = sendto(sender_sockfd, message.data(), message.size(), 0, receiver_sa, sizeof(*receiver_sa));
            if(bytes_sent < 0){
                std::cerr << "SEND FAILED" << std::endl;
            }else{
                //std::cout << "SENDING THE MESSAGE: " << bytes_sent << std::endl;
            }
        }
        //receive to the port binded to the UDP object
        string receive(sockaddr* sender_sa, socklen_t* len){
            ssize_t n = recvfrom(sockfd, buffer, 1024, MSG_WAITALL, sender_sa, len);
            string receivedData(buffer, static_cast<unsigned long>(n));
            return receivedData;
        }
        int getSockfd(){
            return sockfd;
        }
};
