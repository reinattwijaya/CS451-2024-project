#pragma once
#include <string>
#include <UDP.hpp>
#include <sys/select.h>
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <errno.h>
#include "parser.hpp"
#include <chrono>

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
        //acks[process_id][sequence_number] = (numAck, message)
        std::map<std::pair<uint8_t, uint32_t>, std::pair<uint8_t, Message>> acks;
        //p_acks[process_id][sender_id][sequence_number]
        std::map<std::pair<uint8_t, std::pair<uint8_t, uint32_t>>, bool> p_acks;
        std::vector<Parser::Host> hosts;
        std::ofstream& outputFile;
    public:
        std::map<uint8_t, uint32_t> lastDelivered;
        UDP udp;
        /*
        FIFO broadcast has two types of function:
        1. broadcast algorithm that sends the message to all other processes
        2. receive function to determine what to do with the message we have received
        */
        FIFOBroadcast(in_addr_t ip, in_port_t port, uint8_t pid, std::vector<Parser::Host> &_hosts, std::ofstream& outFile): 
            process_id(pid), hosts(_hosts), outputFile(outFile), udp(ip, port){
        }

        void process_receive(uint32_t the_i){

	    struct sockaddr_in sender_sa;
           
	    string recvMessage = udp.receive(reinterpret_cast<sockaddr*>(&sender_sa), &len);
	    
	    //if(recvMessage == "failed"){
	        //std::cout<<"FAILED" <<std::endl;
	    //    break;
	    //}
	    //std::cout << "Z" << std::endl;

	    uint8_t temp_sender_id = stringToUInt8(recvMessage.substr(0, 1));
	    //special message, special treatment, ignore the rest
	    if(temp_sender_id == 0){
	        //std::cout << "XX" << std::endl;
	        uint32_t m_last_delivered[150];
	        unsigned int cnt = 1;
	        for(unsigned int i = 1; i < recvMessage.length();){
	            string temp_msg = "";
	            for(unsigned int j = 0; j < 4; j++)
	                temp_msg += recvMessage[i++];
	            m_last_delivered[cnt++] = stringToUInt32(temp_msg);
	        }
	        //handle special messages
	        for(uint8_t i = 1; i < cnt; i ++){
	            //std::cout << "PROCESS : " << static_cast<unsigned int>(i) << " HAVE DELIVERED: " << static_cast<unsigned int>(m_last_delivered[i]) << " WHILE WE HAVE DELIVERED: " << static_cast<unsigned int>(lastDelivered[i]) << std::endl;
	            while(lastDelivered[i]++ < m_last_delivered[i]){
	                auto it = acks.find(make_pair(i, lastDelivered[i]));
	                if(it == acks.end() || it->second.second.getMessage() == ""){
	                    //if no message has been found, send a NACK
	                    Message nack = Message("", process_id, static_cast<uint8_t>(i + 128), lastDelivered[i], 0);
	                    //std::cout << "SENDING NACK FOR " << static_cast<unsigned int>(nack.getProcessId()) << " OF SEQ NUM " << static_cast<unsigned int>(nack.getSequenceNumber()) << std::endl;
	                    for(unsigned long i = 0; i < hosts.size(); i ++){
	                        memset(&receiver_sa, 0, sizeof(receiver_sa));
	                        receiver_sa.sin_family = AF_INET;
	                        receiver_sa.sin_addr.s_addr = hosts[i].ip;
	                        receiver_sa.sin_port = hosts[i].port;
	                        len = sizeof(receiver_sa);
	                        udp.send(nack.getMessage(), reinterpret_cast<sockaddr*>(&receiver_sa));
	                    }
	                    break;
	                }
	                string msgToDeliver = it->second.second.createDeliveredMessage();
	                //std::cout << "DELIVER MESSAGE BECAUSE OF SPECIAL: " << msgToDeliver << std::endl;
	                outputFile << msgToDeliver;
	                it->second.first = static_cast<uint8_t>(hosts.size()/2+1);
	            }
	            
	            lastDelivered[i]--;
	        }
	        return;
	    }

	    Message m = Message(recvMessage);
	    //std::cout << "RECEIVING (1-ACK,0-NOT): " << m.getIsAck() << " FROM THE PROCESS " << static_cast<unsigned int>(m.getSenderId()) << " OF A MESSAGE FROM " << static_cast<unsigned int>(m.getProcessId()) << " WITH SEQ NUM: " << static_cast<unsigned int>(m.getSequenceNumber()) << std::endl;
	    
	    //for (const auto& pair : acks) {
	    //    std::cout << "Key: (" << static_cast<unsigned int>(pair.first.first) << ", " << static_cast<unsigned int>(pair.first.second) << ")\n";
	    //}
	    
	    auto it = acks.find(make_pair(m.getProcessId(), m.getSequenceNumber()));
	    uint8_t numAck = 0, m_sender_id = m.getSenderId(), m_process_id = m.getProcessId();
	    uint32_t m_seq_num = m.getSequenceNumber();
	    uint8_t hostCutOff = static_cast<uint8_t>(hosts.size()/2);
	    //decreases the number of broadcasts by around 10%, dk if it is worth it but it also reduces some performance so just comment
	    // if(lastDelivered[m_process_id] >= m_seq_num)
	    //     return;

	    //another special case, just make sure to reset back the counter to that specific process if 
	    if(m.getIsNAck()){
	        memset(&receiver_sa, 0, sizeof(receiver_sa));
	        receiver_sa.sin_family = AF_INET;
	        for(unsigned long i = 0; i < hosts.size(); i ++){
	            if(hosts[i].id == m_sender_id){
	                receiver_sa.sin_addr.s_addr = hosts[i].ip;
	                receiver_sa.sin_port = hosts[i].port;
	                break;
	            }
	        }
	        len = sizeof(receiver_sa);
	        if(m_process_id == process_id){
	            //std::cout << "RESENDING MESSAGE FROM " << static_cast<unsigned int>(m_seq_num) <<   " TO PROCESS: " << static_cast<unsigned int>(m_sender_id) << std::endl;
	            string message = "";
	            uint32_t currentSeqNum = m_seq_num;
	            for(uint32_t i = ((m_seq_num-1)*8)+1; i <= the_i; i ++){
	                message += uint32ToString(i);
	                if (i % 8 == 0 || i == the_i){
	                    Message message_to_send = Message(message, process_id, process_id, currentSeqNum, false);
	                    udp.send(message_to_send.getMessage(), reinterpret_cast<sockaddr*>(&receiver_sa));
	                    currentSeqNum++;
	                    message = "";
	                }
	            }
	        }
	        else if(it != acks.end()){
	            //std::cout << "WE HAVE THE MESSAGE SO SEND : " << static_cast<unsigned int>(m_seq_num) << std::endl;
	            udp.send(it->second.second.getMessage(), reinterpret_cast<sockaddr*>(&receiver_sa));
	        }
	        return;
	    }

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
	            }else
	                numAck = it->second.first;
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
	            auto it_p1 = p_acks.find(make_pair(m_process_id, make_pair(m_sender_id, m_seq_num)));
	            if(it_p1 == p_acks.end()){
	                numAck = ++it->second.first;
	                p_acks[make_pair(m_process_id, make_pair(m_sender_id, m_seq_num))] = true;
	            }else
	                numAck = it->second.first;
	            if(it->second.second.getIsAck())
	                it->second.second = m;
	        }
	        //if there is no ack, create the ack
	        else{
	            //std::cout << static_cast<unsigned int>(m_process_id) << ' ' << static_cast<unsigned int>(m_seq_num) << std::endl;
	            //for (const auto& pair : acks) {
	            //    std::cout << "Key: (" << static_cast<unsigned int>(pair.first.first) << ", " << static_cast<unsigned int>(pair.first.second) << ")\n";
	            //}
	            auto res = acks.insert({make_pair(m_process_id, m_seq_num), make_pair(2, m)});
	            it = res.first;
	            //std::cout << "NO ACK: " << ' ' << res.second << std::endl;
	            //std::cout << it->second.second.getIsAck() << std::endl;
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
	    //std::cout << "THE CURRENT NUMACK: " << static_cast<unsigned int>(numAck) << ' ' << static_cast<unsigned int>(hostCutOff) << std::endl;
	    //std::cout << "PROCESSS: " << static_cast<unsigned int>(m_process_id) << " THE CUR SEQ NUM: " << curSeqNum << ' ' << lastDelivered[m_process_id] << std::endl;
	    if(numAck > hostCutOff && lastDelivered[m_process_id]+1 == curSeqNum){
	        string msgToDeliver = it->second.second.createDeliveredMessage();
	        while(numAck > hostCutOff && msgToDeliver != ""){
	            //std::cout << "THE MESSAGE: " << msgToDeliver << " OF SEQ NUM: " << curSeqNum << std::endl;
	            outputFile << msgToDeliver;
	            it = acks.find(make_pair(m.getProcessId(), ++curSeqNum));
	            if(it == acks.end())
	                break;
	            numAck = it->second.first;
	            msgToDeliver = it->second.second.createDeliveredMessage();
	        }
	        //std::cout << "DELIVERED PROCESS: " << static_cast<unsigned int>(m.getProcessId()) << " UNTIL SEQ NUM: " << static_cast<unsigned int>(curSeqNum-1) << std::endl;
	        lastDelivered[m.getProcessId()] = curSeqNum-1;
	    }

        }

        //we broadcast the message to all other processes except the message sender and the current process
        void broadcast(Message message, bool isAckBroadcast = false){
            if(message.getSenderId() == message.getProcessId() && (acks.find(make_pair(message.getProcessId(), message.getSequenceNumber())) == acks.end()))
                acks[make_pair(message.getProcessId(), message.getSequenceNumber())] = make_pair(1, message);
            for(unsigned long i = 0; i < hosts.size(); i ++){
                if((isAckBroadcast && hosts[i].id != message.getProcessId() && hosts[i].id != message.getSenderId()) || hosts[i].id == process_id)
                    continue;
                if(!isAckBroadcast){
                    if(hosts[i].id == message.getSenderId() || hosts[i].id == message.getProcessId() || hosts[i].id == process_id ||
                            (p_acks[make_pair(message.getProcessId(), make_pair(hosts[i].id, message.getSequenceNumber() && message.getIsAck()))]))
                        continue;
                }

                //std::cout << "BROADCASTING FROM PROCESS " << static_cast<unsigned int>(process_id) << " TO " << hosts[i].id << " OF MESSAGE " << static_cast<unsigned int>(message.getProcessId()) <<  " WHICH IS (1=ACK,0=NOTACK): " << isAckBroadcast << std::endl;
                memset(&receiver_sa, 0, sizeof(receiver_sa));
                receiver_sa.sin_family = AF_INET;
                receiver_sa.sin_addr.s_addr = hosts[i].ip;
                receiver_sa.sin_port = hosts[i].port;
                len = sizeof(receiver_sa);
                udp.send(message.getMessage(), reinterpret_cast<sockaddr*>(&receiver_sa));
            }
        }
};
