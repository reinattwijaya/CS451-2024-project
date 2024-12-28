#pragma once
#include <string>
#include <UDP.hpp>
#include <iostream>
#include <vector>
#include <atomic>
#include <thread>
#include <unordered_set>
#include "parser.hpp"

using namespace std;

class LatticeAgreement{
    private:
        vector<unordered_set<uint32_t>> ack_count, nack_count;
        vector<uint32_t> proposed_values;
        vector<vector<uint32_t>> &delivered_values;
        uint32_t active_proposal_number = 0;
        uint32_t current_seq_num = 0;
        bool active = false;
        atomic<bool> is_done = false;
        thread broadcast_thread;

        uint8_t process_id;
        vector<Parser::Host>& hosts;
        ofstream& outputFile;
        UDP udp;
        struct sockaddr_in receiver_sa;
        socklen_t len;
    public:
        LatticeAgreement(in_addr_t ip, in_port_t port, uint8_t pid, vector<Parser::Host> &_hosts, 
                            vector<vector<uint32_t>> &props, ofstream& outFile):
            delivered_values(props), process_id(pid), hosts(_hosts), outputFile(outFile), udp(ip, port){
                for(uint32_t i = 0; i < props.size(); i ++){
                    ack_count.push_back(unordered_set<uint32_t>({process_id}));
                    nack_count.push_back(unordered_set<uint32_t>({}));
                }
            }

        string getDeliveredMessage(){
            string ret = "";
            for(uint32_t i = 0; i < proposed_values.size(); i ++){
                ret += std::to_string(proposed_values[i]) + " ";
            }
            ret += "\n";
            return ret;
        }
        
        void broadcast(bool current_version, Message message){
            while(current_version == is_done.load()){
                for(unsigned long i = 0; i < hosts.size(); i ++){
                    if(hosts[i].id == process_id)
                        continue;
                    memset(&receiver_sa, 0, sizeof(receiver_sa));
                    receiver_sa.sin_family = AF_INET;
                    receiver_sa.sin_addr.s_addr = hosts[i].ip;
                    receiver_sa.sin_port = hosts[i].port;
                    len = sizeof(receiver_sa);
                    udp.send(message.getMessage(), reinterpret_cast<sockaddr*>(&receiver_sa));
                }
                //sleep(1);
            }
        }

        void propose(){
            proposed_values.assign(delivered_values[current_seq_num].begin(), delivered_values[current_seq_num].end());
            active = true;
            active_proposal_number = 0;
            broadcast_thread = thread(&LatticeAgreement::broadcast, this, is_done.load(), 
                                Message(0, process_id, current_seq_num, active_proposal_number, proposed_values));
        }

        bool isSubset(vector<uint32_t> &a, vector<uint32_t> &b){
            return includes(b.begin(), b.end(), a.begin(), a.end());
        }

        void receive(bool isFirst = false, uint32_t maxDistinctValues = INT32_MAX){
            struct sockaddr_in sender_sa;
            while(true){

                if(isFirst){
                    if((ack_count[current_seq_num].size() > hosts.size()/2 || proposed_values.size() == maxDistinctValues) && active){
                        // cout << "DELIVERING" << endl;
                        outputFile << getDeliveredMessage();
                        // cout << current_seq_num << ' ' << active_proposal_number << ' ' << getDeliveredMessage() << endl;
                        delivered_values[current_seq_num] = proposed_values;
                        active = false;
                        current_seq_num++;
                        if (broadcast_thread.joinable()){
                            is_done.store(!is_done.load());
                            broadcast_thread.join();
                        }
                        return;
                    }

                    if(nack_count[current_seq_num].size() > 0 && ack_count[current_seq_num].size() + nack_count[current_seq_num].size() > hosts.size()/2 && active){
                        // cout << "NEW PROPOSAL" << endl;
                        // for(unsigned int i = 0; i < proposed_values.size(); i ++)
                        //     cout << proposed_values[i] << " ";
                        // cout << endl;
                        active_proposal_number++;
                        ack_count[current_seq_num].clear();
                        nack_count[current_seq_num].clear();
                        ack_count[current_seq_num].insert(process_id);
                        if (broadcast_thread.joinable()){
                            is_done.store(!is_done.load());
                            broadcast_thread.join();
                        }
                        broadcast_thread = thread(&LatticeAgreement::broadcast, this, is_done.load(), 
                                        Message(0, process_id, current_seq_num, active_proposal_number, proposed_values));
                    }

                    isFirst = false;
                }

                string recvMessage = udp.receive(reinterpret_cast<sockaddr*>(&sender_sa), &len);

                Message m = Message(recvMessage);

                //there is a delivered value for this sequence number
                vector<uint32_t> &the_proposal = (m.getSequenceNumber() != current_seq_num)
                                     ? delivered_values[m.getSequenceNumber()]
                                     : proposed_values;

                // cout << "THE PROPOSED VALUE: " << getDeliveredMessage() << endl;
                // for(unsigned int i = 0; i < the_proposal.size(); i ++)
                //     cout << the_proposal[i] << " ";
                // cout << endl;
                
                // cout << "MESSAGE SEQ NUM: " << m.getSequenceNumber() << endl;

                if(m.isAck() && m.getActiveProposalNumber() == active_proposal_number){
                    //cout << "GET ACK FROM " << static_cast<unsigned int>(m.getSenderId()) << endl;
                    ack_count[m.getSequenceNumber()].insert(m.getSenderId());
                }
                else if (m.isNAck() && m.getActiveProposalNumber() == active_proposal_number){
                    //cout << "GET NACK FROM " << static_cast<uint32_t>(m.getSenderId()) << endl;
                    nack_count[m.getSequenceNumber()].insert(m.getSenderId());
                    if(m.getSequenceNumber() >= current_seq_num){
                        vector<uint32_t> new_proposal;
                        set_union(the_proposal.begin(), the_proposal.end(), m.proposal.begin(), m.proposal.end(), 
                            back_inserter(new_proposal));
                        the_proposal.assign(new_proposal.begin(), new_proposal.end());
                    }
                }
                else if (m.isProposal()){
                    //cout << "GET PROPOSAL FROM " << static_cast<uint32_t>(m.getSenderId()) << endl;
                    memset(&receiver_sa, 0, sizeof(receiver_sa));

                    receiver_sa.sin_family = AF_INET;
                    receiver_sa.sin_addr.s_addr = hosts[m.getSenderId()-1].ip;
                    receiver_sa.sin_port = hosts[m.getSenderId()-1].port;
                    
                    // for(unsigned int i = 0; i < the_proposal.size(); i ++)
                    //     cout << the_proposal[i] << " ";
                    // cout << endl;
                    if(isSubset(the_proposal, m.proposal)){
                        //cout << "SEND ACK" << endl;
                        the_proposal.assign(m.proposal.begin(), m.proposal.end());
                        udp.send(Message(1, process_id, m.getSequenceNumber(), m.getActiveProposalNumber(), 
                            m.proposal).getMessage(), reinterpret_cast<sockaddr*>(&receiver_sa));
                    }else{
                        //cout << "SEND NACK TO " << static_cast<uint32_t>(m.getSenderId()) << endl;
                        // for(unsigned int i = 0; i < m.proposal.size(); i ++)
                        //     cout << m.proposal[i] << " ";
                        // cout << endl;
                        vector<uint32_t> new_proposal;
                        set_union(the_proposal.begin(), the_proposal.end(), 
                            m.proposal.begin(), m.proposal.end(), back_inserter(new_proposal));
                        the_proposal.assign(new_proposal.begin(), new_proposal.end());
                        udp.send(Message(2, process_id, m.getSequenceNumber(), m.getActiveProposalNumber(), 
                            the_proposal).getMessage(), reinterpret_cast<sockaddr*>(&receiver_sa));
                    }
                }

                // cout << current_seq_num << ' ' << ack_count[current_seq_num].size() << ' ' << nack_count[current_seq_num].size() << endl;
                
                if(ack_count[current_seq_num].size() > hosts.size()/2 && active){
                    // cout << "DELIVERING" << endl;
                    outputFile << getDeliveredMessage();
                    // cout << current_seq_num << ' ' << active_proposal_number << ' ' << getDeliveredMessage() << endl;
                    delivered_values[current_seq_num] = proposed_values;
                    active = false;
                    current_seq_num++;
                    if (broadcast_thread.joinable()){
                        is_done.store(!is_done.load());
                        broadcast_thread.join();
                    }
                    return;
                }

                if(nack_count[current_seq_num].size() > 0 && ack_count[current_seq_num].size() + nack_count[current_seq_num].size() > hosts.size()/2 && active){
                    // cout << "NEW PROPOSAL" << endl;
                    // for(unsigned int i = 0; i < proposed_values.size(); i ++)
                    //     cout << proposed_values[i] << " ";
                    // cout << endl;
                    active_proposal_number++;
                    ack_count[current_seq_num].clear();
                    nack_count[current_seq_num].clear();
                    ack_count[current_seq_num].insert(process_id);
                    if (broadcast_thread.joinable()){
                        is_done.store(!is_done.load());
                        broadcast_thread.join();
                    }
                    broadcast_thread = thread(&LatticeAgreement::broadcast, this, is_done.load(), 
                                    Message(0, process_id, current_seq_num, active_proposal_number, proposed_values));
                }
                // cout << endl;
            }
        }
};

/*
new improvement:
1. if we already reached the maximum number of distinct values, just deliver and move on
2. do a check first before waiting for a message, you might already have everything needed
*/