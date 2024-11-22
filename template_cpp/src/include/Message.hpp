#pragma once
#include <string>

using std::string;

class Message{
    private:
        // we can add metadata to know the type of the message, but right now just having this is fine since we only send integers
        int sequence_number;
        string message;
    public:
        Message(string m): message(m){
            if(m != "0"){
                unsigned int i = 0;
                string sequence_number_str = "";
                while(m[i] != ','){
                    sequence_number_str += m[i];
                    i++;
                }
                int sequence_number = std::stoi(sequence_number_str);
            }
        }
        int getSequenceNumber(){return sequence_number;}
        string getMessage(){return message;}
        string createDeliveredMessage(string process_id){
            string message_to_deliver = "";
            unsigned int i = 0;
            while(message[i] != ','){
                i++;
            }
            for(i++; i < message.length(); i++){
                message_to_deliver = "d " + process_id + " ";
                for(; i < message.length() && message[i] != '\n'; i++){
                    message_to_deliver += message[i];
                }
                message_to_deliver += '\n';
            }
            return message_to_deliver;
        }
};
