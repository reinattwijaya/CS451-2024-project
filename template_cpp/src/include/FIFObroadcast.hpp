#pragma once
#include <string>
#include <UDP.hpp>
#include <sys/select.h>
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include "parser.hpp"

using std::make_pair;

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
        std::map<std::pair<uint8_t, std::pair<uint8_t, uint32_t>>, bool> p_acks;
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
            struct sockaddr_in sender_sa;
            string recvMessage = udp.receive(reinterpret_cast<sockaddr*>(&sender_sa), &len);

            Message m = Message(recvMessage);
            std::cout << "RECEIVING: " << m.getIsAck() << ' ' << static_cast<unsigned int>(m.getSenderId()) <<' ' << static_cast<unsigned int>(m.getProcessId()) << ' ' << static_cast<unsigned int>(m.getSequenceNumber()) << std::endl;
            
            for (const auto& pair : acks) {
                std::cout << "Key: (" << static_cast<unsigned int>(pair.first.first) << ", " << static_cast<unsigned int>(pair.first.second) << ")\n";
            }
            
            auto it = acks.find(make_pair(m.getProcessId(), m.getSequenceNumber()));
            uint8_t numAck = 0, m_sender_id = m.getSenderId(), m_process_id = m.getProcessId();
            uint32_t m_seq_num = m.getSequenceNumber();
            uint8_t hostCutOff = static_cast<uint8_t>(hosts.size()/2);
            //ack message, good, add to your acknowledgement
            if(m.getIsAck()){
                //if there is no ack found for this msg, start a new ack by adding the sender
                //process ack and the message process ack (3 in total) since the sender cannot
                //send ack
                if(it == acks.end()){
                    auto res = acks.insert({make_pair(m_process_id, m_seq_num), make_pair(3, m)});
                    it = res.first;
                    numAck = 3;
                    p_acks[make_pair(m_process_id, make_pair(m_process_id, m_seq_num))] = true;
                    p_acks[make_pair(m_process_id, make_pair(m_sender_id, m_seq_num))] = true;
                }
                else{
                    auto it_p1 = p_acks.find(make_pair(m_process_id, make_pair(m_sender_id, m_seq_num)));
                    //if the sender process ack has not been processed, process it
                    if(it_p1 == p_acks.end()){
                    	 numAck = ++it->second.first;
                    	 p_acks[make_pair(m_process_id, make_pair(m_sender_id, m_seq_num))] = true;
                    }
                }
            }
            //real message, rebroadcast it and send acknowledgement
            else {
                if(m.getSenderId() == m.getProcessId()){
                    m.changeSenderId(process_id);
                    broadcast(m);
                }
                Message ack = Message("", process_id, m.getProcessId(), m.getSequenceNumber(), 1);
                broadcast(ack, true);
                //if there is an ack, replace the message to have the actual message
                if(it != acks.end()){
                    numAck = it->second.first;
                    if(it->second.second.getIsAck())
                        it->second.second = m;
                }
                //if there is no ack, create the ack
                else{
                    std::cout << static_cast<unsigned int>(m_process_id) << ' ' << static_cast<unsigned int>(m_seq_num) << std::endl;
                    for (const auto& pair : acks) {
                        std::cout << "Key: (" << static_cast<unsigned int>(pair.first.first) << ", " << static_cast<unsigned int>(pair.first.second) << ")\n";
                    }
                    auto res = acks.insert({make_pair(m_process_id, m_seq_num), make_pair(2, m)});
                    it = res.first;
                    std::cout << "NO ACK: " << ' ' << res.second << std::endl;
                    std::cout << it->second.second.getIsAck() << std::endl;
                    if(m_process_id == m_sender_id)
                        numAck = 2;
                    else
                        //if sender is different from process, the message is sent from other
                        //process and we can be sure that process has acknowledged it
                        numAck = 3;
                    p_acks[make_pair(m_process_id, make_pair(m_process_id, m_seq_num))] = true;
                    p_acks[make_pair(m_process_id, make_pair(m_sender_id, m_seq_num))] = true;
                }
            }
            uint32_t curSeqNum = m_seq_num;
            std::cout << "THE CURRENT NUMACK: " << static_cast<unsigned int>(numAck) << ' ' << static_cast<unsigned int>(hostCutOff) << std::endl;
            std::cout << "THE CUR SEQ NUM: " << curSeqNum << ' ' << lastDelivered[m_process_id] << std::endl;
            if(numAck > hostCutOff && lastDelivered[m_process_id]+1 == curSeqNum){
                string msgToDeliver = it->second.second.createDeliveredMessage();
                std::cout << "THE MESSAGE: " << msgToDeliver << std::endl;
                while(numAck > hostCutOff && msgToDeliver != ""){
                    outputFile << msgToDeliver;
                    it = acks.find(make_pair(m.getProcessId(), ++curSeqNum));
                    if(it == acks.end())
                        break;
                    numAck = it->second.first;
                    msgToDeliver = it->second.second.createDeliveredMessage();
                }
                lastDelivered[m.getProcessId()] = curSeqNum-1;
            }
        }

        //we broadcast the message to all other processes except the message sender and the current process
        void broadcast(Message message, bool sendToSender = false){
            if(message.getSenderId() == message.getProcessId())
                acks[make_pair(message.getProcessId(), message.getSequenceNumber())] = make_pair(1, message);
            for(unsigned long i = 0; i < hosts.size(); i ++){
                std::cout << "BROADCASTING: " << static_cast<unsigned int>(process_id) << ' ' << hosts[i].id << ' ' << static_cast<unsigned int>(message.getProcessId()) <<  ' ' << sendToSender << std::endl;
                if((!sendToSender && hosts[i].id == message.getProcessId()) || hosts[i].id == process_id)
                    continue;
                std::cout << hosts[i].ip << ' ' << hosts[i].port << std::endl;
                memset(&receiver_sa, 0, sizeof(receiver_sa));
                receiver_sa.sin_family = AF_INET;
                receiver_sa.sin_addr.s_addr = hosts[i].ip;
                receiver_sa.sin_port = hosts[i].port;
                len = sizeof(receiver_sa);
                udp.send(message, reinterpret_cast<sockaddr*>(&receiver_sa));
            }
        }
};
