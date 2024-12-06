#pragma once
#include <string>

using std::string;

string uint32ToString(uint32_t n);
string uint8ToString(uint8_t n);
uint8_t stringToUInt8(string s);
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
uint8_t stringToUInt8(string s){
    return static_cast<uint8_t>(s[0]);
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
        uint8_t sender_id;
        uint8_t process_id;
        uint32_t sequence_number;
        bool isAck;
        string message;
    public:
        Message(){
            sender_id = 0;
            process_id = 0;
            sequence_number = 0;
            isAck = false;
            message = "";
        }
        Message(string m): message(m){
            if(m.length() <= 6)
                isAck = true;
            else
                isAck = false;
            sender_id = stringToUInt8(m.substr(0, 1));
            process_id = stringToUInt8(m.substr(1, 1));
            sequence_number = stringToUInt32(m.substr(2, 4));
        }
        Message(string m, uint8_t sid, uint8_t pid, uint32_t seq_num, bool _isAck): sender_id(sid), process_id(pid), sequence_number(seq_num), isAck(_isAck){
            message = uint8ToString(sid) + uint8ToString(pid) + uint32ToString(seq_num) + m;
        }
        void changeSenderId(uint8_t new_sid){
            sender_id = new_sid;
            message[0] = static_cast<char>(new_sid & 0xFF);
        }
        uint32_t getSequenceNumber(){return sequence_number;}
        uint8_t getSenderId(){return sender_id;}
        uint8_t getProcessId(){return process_id;}
        bool getIsAck(){return isAck;}
        string getMessage(){return message;}
        string createDeliveredMessage(){
            string message_to_deliver = "";
            for(unsigned int i = 6; i < message.length();){
                message_to_deliver += "d " + std::to_string(static_cast<unsigned int>(process_id)) + " ";
                string temp_msg = "";
                for(unsigned int j = 0; j < 4; j++)
                    temp_msg += message[i++];
                message_to_deliver += std::to_string(static_cast<unsigned int>(stringToUInt32(temp_msg))) + '\n';
            }
            return message_to_deliver;
        }
};
