#pragma once
#include <string>
#include <vector>
#include <iostream>

using std::string;
using std::vector;

string uint32ToString(uint32_t n);
string uint8ToString(uint8_t n);
uint8_t stringToUInt8(char s);
uint32_t stringToUInt32(string s);

string uint32ToString(uint32_t n){
    string ret = "";
    ret += static_cast<char>((n >> 24) & 0xFF);
    ret += static_cast<char>((n >> 16) & 0xFF);
    ret += static_cast<char>((n >> 8) & 0xFF);
    ret += static_cast<char>(n & 0xFF);
    return ret;
}
string uint8ToString(uint8_t n){
    string ret = "";
    ret += static_cast<char>(n & 0xFF);
    return ret;
}
uint8_t stringToUInt8(char s){
    return static_cast<uint8_t>(s);
}
uint32_t stringToUInt32(string s){
    return (static_cast<uint32_t>(static_cast<unsigned char>(s[0])) << 24) |
           (static_cast<uint32_t>(static_cast<unsigned char>(s[1])) << 16) |
           (static_cast<uint32_t>(static_cast<unsigned char>(s[2])) << 8)  |
           (static_cast<uint32_t>(static_cast<unsigned char>(s[3])));
}

class Message{
    private:
        // we can add metadata to know the type of the message, but right now just having this is fine since we only send integers
        // message type -> 0: proposal, 1: ack, 2: nack
        uint8_t message_type, sender_id;
        uint32_t sequence_number, active_proposal_number;
        string message;
    public:
        vector<uint32_t> proposal;
        Message(){
            message_type = 0;
            sender_id = 0;
            sequence_number = 0;
            active_proposal_number = 0;
            message = "";
        }
        Message(string m): message(m){
            message_type = stringToUInt8(m[0]);
            sender_id = stringToUInt8(m[1]);
            sequence_number = stringToUInt32(m.substr(2, 4));
            active_proposal_number = stringToUInt32(m.substr(6, 4));
            if(message_type != 1){
                for(uint32_t i = 10; i < m.size(); i += 4){
                    string temp = "";
                    for(uint32_t j = 0; j < 4; j ++)
                        temp += m[i+j];
                    proposal.push_back(stringToUInt32(temp));
                }
            }
        }
        Message(uint8_t msg_type, uint8_t sid, uint32_t seq_num, uint32_t active_prop, vector<uint32_t> &prop):
            message_type(msg_type), sender_id(sid), sequence_number(seq_num), active_proposal_number(active_prop){
            message = uint8ToString(msg_type) + uint8ToString(sid) + uint32ToString(seq_num) + uint32ToString(active_prop);
            if(message_type != 1){
                for(uint32_t i = 0; i < prop.size(); i ++)
                    message += uint32ToString(prop[i]);
                proposal.assign(prop.begin(), prop.end());
            }
        }
        uint32_t getSequenceNumber(){return sequence_number;}
        bool isAck(){return message_type == 1;}
        bool isNAck(){return message_type == 2;}
        bool isProposal(){return message_type == 0;}
        uint8_t getSenderId(){return sender_id;}
        uint32_t getActiveProposalNumber(){return active_proposal_number;}

        string getMessage(){return message;}
};
