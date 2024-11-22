#pragma once
#include <Message.hpp>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <cstring>

class UDP{
    private:
        int sockfd;
        struct sockaddr_in process_sa;
    public:
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
        }
        void send(Message message, const sockaddr* receiver_sa){
            std::string fullMessage = message.getFullMessage();
            sendto(sockfd, fullMessage.c_str(), fullMessage.size(), 0, receiver_sa, sizeof(*receiver_sa));
        }
        void receive(char* buffer, unsigned short buffer_len, sockaddr* sender_sa, socklen_t* len){
            ssize_t n = recvfrom(sockfd, reinterpret_cast<char*>(buffer), buffer_len, MSG_WAITALL, reinterpret_cast<sockaddr*>(sender_sa), len);
            buffer[n] = '\0';
        }
        int getSockfd(){
            return sockfd;
        }
};
