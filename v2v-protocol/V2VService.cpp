/*
* @authors- Andrius Sakalas, Margit Saal, Karanveer Singh
* Group - 12
*/
#include "V2VService.hpp"
#include <pthread.h>
#include <iostream>
// Global Variables
opendlv::proxy::GroundSteeringReading carSteering;
opendlv::proxy::PedalPositionReading carSpeed;


int main() {

    std::shared_ptr<V2VService> v2vService = std::make_shared<V2VService>();

    // Getting values from the PS4 Controller 
    cluon::OD4Session od4(230,[](cluon::data::Envelope &&envelope) noexcept {
        if (envelope.dataType() == opendlv::proxy::GroundSteeringReading::ID()) {
            opendlv::proxy::GroundSteeringReading receivedMsg = cluon::extractMessage<opendlv::proxy::GroundSteeringReading>(std::move(envelope));
            GROUND_STEERING = receivedMsg.steeringAngle();
          }
        if (envelope.dataType() == opendlv::proxy::PedalPositionReading::ID()) {
            opendlv::proxy::PedalPositionReading receivedMsg = cluon::extractMessage<opendlv::proxy::PedalPositionReading>(std::move(envelope));
            PEDAL_SPEED = receivedMsg.percent();
          }
	   });
    

    while (1) {
        int choice;
        char quit;
        std::string groupId;
        std::cout << "Which message would you like to send?" << std::endl;
        std::cout << "(1) AnnouncePresence" << std::endl;
        std::cout << "(2) FollowRequest" << std::endl;
        std::cout << "(3) FollowResponse" << std::endl;
        std::cout << "(4) StopFollow" << std::endl;
        std::cout << "(5) LeaderStatus" << std::endl;
        std::cout << "(6) FollowerStatus" << std::endl;
        std::cout << "(#) Nothing, just quit." << std::endl;
        std::cout << ">> ";
        std::cin >> choice;

        switch (choice) {
            case 1: v2vService->announcePresence(); break;
            case 2: {
                std::cout << "Which group do you want to follow?" << std::endl;
                std::cin >> groupId;
                if (v2vService->presentCars.find(groupId) != v2vService->presentCars.end())
                    v2vService->followRequest(v2vService->presentCars[groupId]);
                else std::cout << "Sorry, unable to locate that groups vehicle!" << std::endl;
                break;
            }
            case 3: v2vService->followResponse(); break;
            case 4: {
                std::cout << "Which group do you want to stop follow?" << std::endl;
                std::cin >> groupId;
                if (v2vService->presentCars.find(groupId) != v2vService->presentCars.end())
                    v2vService->stopFollow(v2vService->presentCars[groupId]);
                else std::cout << "Sorry, unable to locate that groups vehicle!" << std::endl;
                break;
            }
            case 5: { // Sending Leader Status Values to Follower Car   
            while(1){ 
                      v2vService->leaderStatus(PEDAL_SPEED,GROUND_STEERING,100);	      
			                usleep(125000);  // Sending leader status every 125ms
                    }
                  break;
                }
            case 6: v2vService->followerStatus(); break;
            default: exit(0);
        }
    }
}

/**
 * Implementation of the V2VService class as declared in V2VService.hpp
 */
V2VService::V2VService() {
    /*
     * The movement field contains a reference to the internal channel which is an OD4Session. This is where
     * car speed and steering angle will be broadcasted to the motors.
     */
    movement = 
      std::make_shared<cluon::OD4Session>(INTERNAL_CHANNEL, [this](cluon::data::Envelope &&envelope) noexcept {
            if (envelope.dataType() == opendlv::proxy::GroundSteeringReading::ID()) {
            opendlv::proxy::GroundSteeringReading receivedMsg = cluon::extractMessage<opendlv::proxy::GroundSteeringReading>(std::move(envelope));
            GROUND_STEERING = receivedMsg.steeringAngle();
            }
            if (envelope.dataType() == opendlv::proxy::PedalPositionReading::ID()) {
            opendlv::proxy::PedalPositionReading receivedMsg = cluon::extractMessage<opendlv::proxy::PedalPositionReading>(std::move(envelope));
            PEDAL_SPEED = receivedMsg.percent();
            }
    });

    /*
     * The broadcast field contains a reference to the broadcast channel which is an OD4Session. This is where
     * AnnouncePresence messages will be received.
     */
    broadcast =
        std::make_shared<cluon::OD4Session>(BROADCAST_CHANNEL,
          [this](cluon::data::Envelope &&envelope) noexcept {
              std::cout << "[OD4] ";
              switch (envelope.dataType()) {
                  case ANNOUNCE_PRESENCE: {
                      AnnouncePresence ap = cluon::extractMessage<AnnouncePresence>(std::move(envelope));
                      std::cout << "received 'AnnouncePresence' from '"
                                << ap.vehicleIp() << "', GroupID '"
                                << ap.groupId() << "'!" << std::endl;

                      presentCars[ap.groupId()] = ap.vehicleIp();

                      break;
                  }
                  default: std::cout << "¯\\_(ツ)_/¯" << std::endl;
              }
          });

    /*
     * The internalService field contains a reference to the internal channel which is an OD4Session. This is where
     * we redirect the V2V protocol messages to the web signal viewer.
     */
      internalService = 
        std::make_shared<cluon::OD4Session>(INTERNAL_CHANNEL,[this](cluon::data::Envelope &&envelope) noexcept {
            // internal service
              if(envelope.dataType()==1001) {
                      AnnouncePresence ap = cluon::extractMessage<AnnouncePresence>(std::move(envelope));
                      presentCars[ap.groupId()] = ap.vehicleIp();
                  }
	             else if(envelope.dataType()==2001) {
		                LeaderStatus leaderStatus = cluon::extractMessage <LeaderStatus>(std::move(envelope));
		              }
               else if(envelope.dataType()==1002){
                    FollowRequest followRequest = cluon::extractMessage <FollowRequest>(std::move(envelope));
               }
               else if(envelope.dataType()==1003){
                    FollowResponse followResponse = cluon::extractMessage <FollowResponse>(std::move(envelope));
               }
               else if(envelope.dataType()==1004){
                    StopFollow stopFollow = cluon::extractMessage <StopFollow>(std::move(envelope));
               }

        });

    /*
     * Each car declares an incoming UDPReceiver for messages directed at them specifically. This is where messages
     * such as FollowRequest, FollowResponse, StopFollow, etc. are received.
     */
    incoming =
        std::make_shared<cluon::UDPReceiver>("0.0.0.0", DEFAULT_PORT,
           [this](std::string &&data, std::string &&sender, std::chrono::system_clock::time_point &&ts) noexcept {
               std::cout << "[UDP] ";
               std::pair<int16_t, std::string> msg = extract(data);

               switch (msg.first) {
                   case FOLLOW_REQUEST: {
                       FollowRequest followRequest = decode<FollowRequest>(msg.second);
                       std::cout << "received '" << followRequest.LongName()
                                 << "' from '" << sender << "'!" << std::endl;

                       // After receiving a FollowRequest, check first if there is currently no car already following.
                       if (followerIp.empty()) {
                           unsigned long len = sender.find(':');    // If no, add the requester to known follower slot
                           followerIp = sender.substr(0, len);      // and establish a sending channel.
                           toFollower = std::make_shared<cluon::UDPSender>(followerIp, DEFAULT_PORT);
                           followResponse();
                       }
                       break;
                   }
                   case FOLLOW_RESPONSE: {
                       FollowResponse followResponse = decode<FollowResponse>(msg.second);
                       std::cout << "received '" << followResponse.LongName()
                                 << "' from '" << sender << "'!" << std::endl;
                       break;
                   }
                   case STOP_FOLLOW: {
                       StopFollow stopFollow = decode<StopFollow>(msg.second);
                       std::cout << "received '" << stopFollow.LongName()
                                 << "' from '" << sender << "'!" << std::endl;

                       // Clear either follower or leader slot, depending on current role.
                       unsigned long len = sender.find(':');
                       if (sender.substr(0, len) == followerIp) {
                           followerIp = "";
                           toFollower.reset();
                       }
                       else if (sender.substr(0, len) == leaderIp) {
                           leaderIp = "";
                           toLeader.reset();
                       }
                       break;
                   }
                   case FOLLOWER_STATUS: {
                       FollowerStatus followerStatus = decode<FollowerStatus>(msg.second);
                       std::cout << "received '" << followerStatus.LongName()
                                 << "' from '" << sender << "'!" << std::endl;
                       break;
                   }
                   case LEADER_STATUS: {
                       LeaderStatus leaderStatus = decode<LeaderStatus>(msg.second);

                       std::cout << "Received speed from Leader:'" << leaderStatus.speed()
                                 << "' from '" << sender << "'!" << std::endl;
                       std::cout << "Received angle from Leader:'" << leaderStatus.steeringAngle()
                                 << "' from '" << sender << "'!" << std::endl;

                       carQueue(leaderStatus);    // Sends leaderStatus to the Car Queue
                       break;
                   }
                   default: std::cout << "¯\\_(ツ)_/¯" << std::endl;
               }
           });

}


void V2VService::carQueue(LeaderStatus leaderStatus){
    if(leaderStatus.speed() !=0 && angleQueue.size() >= DELAY_ANGLE){     // Checks if the leader car is moving and certain amount of messages are delayed
      angleQueue.push(leaderStatus.steeringAngle());      // Push leader car's steering angle to the Queue
      carSpeed.percent(leaderStatus.speed());             // Set speed equal to leader car's speed
      carSteering.steeringAngle(angleQueue.front());      // Set steering angle to the first value in the Queue
      movement->send(carSpeed);                           // Send speed to the motors
      movement->send(carSteering);                        // Send angle to the motors
      angleQueue.pop();                                   // Pop the Queue
    } else if(leaderStatus.speed() != 0) {                //Checks if the leader car is moving but the delay is not sufficient 
      carSpeed.percent(leaderStatus.speed());           
      angleQueue.push(leaderStatus.steeringAngle());      // Add Steering angle to the Queue
      movement->send(carSpeed);                           // Sends speed to motors equal to the leader speed
    } else {          // If the leader is not movivng, we set the steering angle and speed to zero and dont add anything in the Queue.
      carSpeed.percent(0);
      carSteering.steeringAngle(0);
      movement->send(carSpeed);
      movement->send(carSteering);
    }
}

/**
 * This function sends an AnnouncePresence (id = 1001) message on the broadcast channel. It will contain information
 * about the sending vehicle, including: IP, port and the group identifier.
 */
void V2VService::announcePresence() {
    if (!followerIp.empty()) return;
    AnnouncePresence announcePresence;
    announcePresence.vehicleIp(YOUR_CAR_IP);
    announcePresence.groupId(YOUR_GROUP_ID);
    broadcast->send(announcePresence);
    internalService->send(announcePresence);    // Sends announce presence to the internal service 
}

/**
 * This function sends a FollowRequest (id = 1002) message to the IP address specified by the parameter vehicleIp. And
 * sets the current leaderIp field of the sending vehicle to that of the target of the request.
 *
 * @param vehicleIp - IP of the target for the FollowRequest
 */
void V2VService::followRequest(std::string vehicleIp) {
    if (!leaderIp.empty()) return;
    leaderIp = vehicleIp;
    toLeader = std::make_shared<cluon::UDPSender>(leaderIp, DEFAULT_PORT);
    FollowRequest followRequest;
    toLeader->send(encode(followRequest));
    internalService->send(followRequest);
}

/**
 * This function send a FollowResponse (id = 1003) message and is sent in response to a FollowRequest (id = 1002).
 * This message will contain the NTP server IP for time synchronization between the target and the sender.
 */
void V2VService::followResponse() {
    if (followerIp.empty()) return;
    FollowResponse followResponse;
    toFollower->send(encode(followResponse));
    internalService->send(followResponse);
}

/**
 * This function sends a StopFollow (id = 1004) request on the ip address of the parameter vehicleIp. If the IP address is neither
 * that of the follower nor the leader, this function ends without sending the request message.
 *
 * @param vehicleIp - IP of the target for the request
 */
void V2VService::stopFollow(std::string vehicleIp) {
    StopFollow stopFollow;
    if (vehicleIp == leaderIp) {
        toLeader->send(encode(stopFollow));
        internalService->send(stopFollow);
        leaderIp = "";
        toLeader.reset();
    }
    if (vehicleIp == followerIp) {
        toFollower->send(encode(stopFollow));
        internalService->send(stopFollow);
        followerIp = "";
        toFollower.reset();
    }
}

/**
 * This function sends a FollowerStatus (id = 3001) message on the leader channel.
 *
 * @param speed - current velocity
 * @param steeringAngle - current steering angle
 * @param distanceFront - distance to nearest object in front of the car sending the status message
 * @param distanceTraveled - distance traveled since last reading
 */
void V2VService::followerStatus() {
    if (leaderIp.empty()) return;
    FollowerStatus followerStatus;
    toLeader->send(encode(followerStatus));
}

/**
 * This function sends a LeaderStatus (id = 2001) message on the follower channel.
 *
 * @param speed - current velocity
 * @param steeringAngle - current steering angle
 * @param distanceTraveled - distance traveled since last reading
 */
void V2VService::leaderStatus(float speed,float steeringAngle, uint8_t distanceTraveled) {
    if (followerIp.empty()) return;
    LeaderStatus leaderStatus;
    leaderStatus.timestamp(getTime());
    leaderStatus.speed(PEDAL_SPEED);
    leaderStatus.steeringAngle(GROUND_STEERING);
    leaderStatus.distanceTraveled(distanceTraveled);
    toFollower->send(encode(leaderStatus));
    internalService->send(leaderStatus);    // Sends Leader Status values to the internal service
}

/**
 * Gets the current time.
 *
 * @return current time in milliseconds
 */
uint32_t V2VService::getTime() {
    timeval now;
    gettimeofday(&now, nullptr);
    return (uint32_t ) now.tv_usec / 1000;
}

/**
 * The extraction function is used to extract the message ID and message data into a pair.
 *
 * @param data - message data to extract header and data from
 * @return pair consisting of the message ID (extracted from the header) and the message data
 */
std::pair<int16_t, std::string> V2VService::extract(std::string data) {
    if (data.length() < 10) return std::pair<int16_t, std::string>(-1, "");
    int id, len;
    std::stringstream ssId(data.substr(0, 4));
    std::stringstream ssLen(data.substr(4, 10));
    ssId >> std::hex >> id;
    ssLen >> std::hex >> len;
    return std::pair<int16_t, std::string> (
            data.length() -10 == len ? id : -1,
            data.substr(10, data.length() -10)
    );
};

/**
 * Generic encode function used to encode a message before it is sent.
 *
 * @tparam T - generic message type
 * @param msg - message to encode
 * @return encoded message
 */
template <class T>
std::string V2VService::encode(T msg) {
    cluon::ToProtoVisitor v;
    msg.accept(v);
    std::stringstream buff;
    buff << std::hex << std::setfill('0')
         << std::setw(4) << msg.ID()
         << std::setw(6) << v.encodedData().length()
         << v.encodedData();
    return buff.str();
}

/**
 * Generic decode function used to decode an incoming message.
 *
 * @tparam T - generic message type
 * @param data - encoded message data
 * @return decoded message
 */
template <class T>
T V2VService::decode(std::string data) {
    std::stringstream buff(data);
    cluon::FromProtoVisitor v;
    v.decodeFrom(buff);
    T tmp = T();
    tmp.accept(v);
    return tmp;
}
