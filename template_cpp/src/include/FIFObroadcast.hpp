#pragma once
#include <string>
#include <UDP.hpp>
#include <sys/select.h>
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include "parser.hpp"

struct pair_hash {
    template <class T1, class T2>
    std::size_t operator () (const std::pair<T1,T2> &p) const {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);

        return h1 ^ h2;  
    }
};

using TheMessageMap = std::pair<unsigned short, unsigned long>;

class FIFOBroadcast{
    private:
        fd_set socks;
        struct timeval t;
        uint8_t process_id;
        socklen_t len;
        struct sockaddr_in receiver_sa;
        std::map<std::pair<uint8_t, uint32_t>, std::pair<uint8_t, Message>> acks;
        std::map<uint8_t, uint32_t> lastDelivered;
        std::vector<Parser::Host> hosts;
        std::ofstream& outputFile;
    public:
        UDP udp;
        /*
        FIFO broadcast has two types of function:
        1. broadcast algorithm that sends the message to all other processes
        2. receive function to determine what to do with the message we have received
        */
        FIFOBroadcast(in_addr_t ip, in_port_t port, uint8_t pid, std::vector<Parser::Host> &_hosts, std::ofstream& outFile): 
            process_id(pid), hosts(_hosts), outputFile(outFile), udp(ip, port){
        }

        void process_receive(){
            char buffer[1024];
            struct sockaddr_in sender_sa;
            udp.receive(buffer, 1024, reinterpret_cast<sockaddr*>(&sender_sa), &len);

            Message m = Message(buffer);
            //ack message, good, add to your acknowledgement
            if(m.getIsAck()){
                auto it = acks.find(std::make_pair(m.getProcessId(), m.getSequenceNumber()));
                uint8_t numAck = 0;
                uint8_t hostCutOff = static_cast<uint8_t>(hosts.size()/2);
                if(it == acks.end()){
                    acks[std::make_pair(m.getProcessId(), m.getSequenceNumber())] = std::make_pair(1, m);
                    numAck = 1;
                }
                else
                    numAck = ++it->second.first;
                string msgToDeliver = it->second.second.createDeliveredMessage();
                uint32_t curSeqNum = m.getSequenceNumber();
                if(numAck > hostCutOff && lastDelivered[m.getProcessId()]+1 == curSeqNum){
                    while(numAck > hostCutOff){
                        outputFile << msgToDeliver;
                        numAck = acks.find(std::make_pair(m.getProcessId(), ++curSeqNum))->second.first;
                    }
                    lastDelivered[m.getProcessId()] = curSeqNum-1;
                }
            }
            //real message, rebroadcast it and send acknowledgement
            else {
                broadcast(m);
                Message ack = Message("", process_id, m.getSequenceNumber(), 1);
                broadcast(ack, true);
                auto it = acks.find(std::make_pair(m.getProcessId(), m.getSequenceNumber()));
                if(it == acks.end()){
                    it->second.first ++;
                    it->second.second = m;
                }
                else
                    acks[std::make_pair(m.getProcessId(), m.getSequenceNumber())] = std::make_pair(1, m);
            }
        }

        //we broadcast the message to all other processes except the message sender and the current process
        void broadcast(Message message, bool sendToSender = false){
            for(unsigned long i = 0; i < hosts.size(); i ++){
                if((!sendToSender && hosts[i].id == message.getProcessId()) || hosts[i].id == process_id)
                    continue;
                memset(&receiver_sa, 0, sizeof(receiver_sa));
                receiver_sa.sin_family = AF_INET;
                receiver_sa.sin_addr.s_addr = hosts[i].ip;
                receiver_sa.sin_port = hosts[i].port;
                len = sizeof(receiver_sa);
                udp.send(message, reinterpret_cast<sockaddr*>(&receiver_sa));
            }
        }
};
