#pragma once
#include <string>

class Message{
    private:
        // we can add metadata to know the type of the message, but right now just having this is fine since we only send integers
        int sequence_number;
        std::string message;
    public:
        Message(int s, std::string m): sequence_number(s), message(m){}
        int getSequenceNumber(){return sequence_number;}
        std::string getMessage(){return message;}
        std::string getFullMessage(){
            return std::to_string(sequence_number) + "," + message;
        }
        std::string createDeliveredMessage(std::string process_id){
            std::string message_to_deliver = "";
            for(unsigned long i = 0; i < message.length(); i++){
                message_to_deliver = "d " + process_id + " ";
                for(; i < message.length() && message[i] != '\n'; i++){
                    message_to_deliver += message[i];
                }
                message_to_deliver += '\n';
            }
            return message_to_deliver;
        }
};

inline Message parseMessage(std::string buffer){
    unsigned long i = 0;
    std::string sequence_number_str = "";
    while(buffer[i] != ','){
        sequence_number_str += buffer[i];
        i++;
    }
    int sequence_number = std::stoi(sequence_number_str);
    i++;
    std::string message = "";
    for(i; i < buffer.length(); i++){
        message += buffer[i];
    }
    return Message(sequence_number, message);
}
