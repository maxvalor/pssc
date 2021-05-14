/*
 * SubscribeMessage.h
 *
 *  Created on: Apr 25, 2021
 *      Author: ubuntu
 */

#ifndef INCLUDE_PSSC_PROTOCOL_MSGS_SUBSCRIBEMESSAGE_H_
#define INCLUDE_PSSC_PROTOCOL_MSGS_SUBSCRIBEMESSAGE_H_

#include "PSSCMessage.h"

namespace pssc {

class SubscribeMessage : public PSSCMessage
{
public:
    // | INS | ID | SUBSCRIBER_ID | SIZE_OF_TOPIC | TOPIC |
    static const pssc_ins INS = Ins::SUBSCRIBE;
    static const pssc_size SIZE_OF_MESSAGE_NECCESSARY =
            SIZE_OF_PSSC_INS + SIZE_OF_PSSC_ID * 2 + SIZE_OF_SIZE;

    pssc_id subscriberId;
    std::string topic;

    SubscribeMessage() = default; // @suppress("Class members should be properly initialized")

    SubscribeMessage(std::shared_ptr<TCPMessage> msg)
    {
        // INS has been taken
        msg->NextData(messageId);
        msg->NextData(subscriberId);
        msg->NextData(topic);
    }

    std::shared_ptr<TCPMessage> toTCPMessage() override
    {
        auto msg = TCPMessage::Generate(SIZE_OF_MESSAGE_NECCESSARY + topic.size());
        msg->AppendData(INS);
        msg->AppendData(messageId);
        msg->AppendData(subscriberId);
        msg->AppendData(topic);
        return msg;
    }
};

}



#endif /* INCLUDE_PSSC_PROTOCOL_MSGS_SUBSCRIBEMESSAGE_H_ */
