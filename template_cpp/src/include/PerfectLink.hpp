#pragma once
#include <string>
#include <UDP.hpp>
#include <sys/select.h>
#include <iostream>

class PerfectLink{
    private:
        fd_set socks;
        struct timeval t;
        int time;
        socklen_t len;
        unsigned int totalLost;
    public:
        UDP udp;
        PerfectLink(in_addr_t ip, in_port_t port, int _time): udp(ip, port) ,time(_time), totalLost(0){
            FD_ZERO(&socks);
            FD_SET(udp.getSockfd(), &socks);
            t.tv_sec = 0;
            t.tv_usec = time;
        }
        void send(Message message, sockaddr* receiver_sa, char* buffer, unsigned short buffer_len){
            len = sizeof(*receiver_sa);
            udp.send(message, receiver_sa);
            while(true){
                int select_result = select(udp.getSockfd() + 1, &socks, NULL, NULL, &t);
                if(select_result == 0){
                    totalLost++;
                    if(totalLost > 1000)
                        std::cout << totalLost << std::endl;
                    udp.send(message, receiver_sa);
                    time = 2*time;
                    t.tv_usec = time;
                }else if(select_result < 0){
                    perror("select failed");
                    break;
                }else{
                    break;
                }
            }
            udp.receive(buffer, buffer_len, receiver_sa, &len);
        }
        Message receive(char* buffer, unsigned short buffer_len, sockaddr* sender_sa){
            len = sizeof(*sender_sa);
            udp.receive(buffer, buffer_len, sender_sa, &len);
            return Message(buffer);
        }
};