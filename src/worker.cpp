#include <iomanip>
#include <iostream>
#include <thread>
#include <memory>
#include <functional>

#include "functions.hpp"
#include "timer.hpp"

struct Worker {
    Worker(int id, int pull_port, int push_port, int heartbeat_port, int heartbeat_time) :
        context(1),
        pull_socket(context, ZMQ_PULL),
        push_socket(context, ZMQ_PUSH),
        heartbeat_socket(context, ZMQ_PUSH),
        my_id(id),
        heartbeat_time(heartbeat_time)
    {
        pull_socket.connect(get_port_name(pull_port));
        push_socket.connect(get_port_name(push_port));
        heartbeat_socket.connect(get_port_name(heartbeat_port));
    }

    void Work() {
        std::string request_string;
        request_string = recieve_message(pull_socket);
        std::string message = "Ok:" + std::to_string(getpid());
        send_message(push_socket, message);
                
        Timer timer;
        while (true) {
            request_string = recieve_message(pull_socket);
            
            std::istringstream cmd_stream(request_string);
            std::string prefix;
            prefix = get_prefix(cmd_stream);
            std::string cmd;
            cmd_stream >> cmd;
            
            bool sent = false;
            if (cmd == "exec") {
                int id;
                std::string type;
                cmd_stream >> id >> type;
                std::string message = prefix + " Ok:" + std::to_string(id);;
                
                if (type == "start") {
                    timer.start();
                    sent = send_message(push_socket, message);
                }
                else if (type == "stop") {
                    timer.stop();
                    sent = send_message(push_socket, message);
                }
                else if (type == "time") {
                    int time = timer.time();
                    message += ": " + std::to_string(time);
                    sent = send_message(push_socket, message);
                }
            }
            else if (cmd == "heartbeat") {
                int time;
                cmd_stream >> time;
                heartbeat_time = time;
            }
            if (!sent && cmd != "heartbeat") {
                std::cout << "Something going wrong" << std::endl;
            }
        }
    }

    void Heartbeat() {
        Timer timer;
        int timer_zero = 0;
        timer.start();
        while (true) {
            while (!heartbeat_time)
                ;
            if (timer.time() - timer_zero < heartbeat_time) {
                continue;
            }
            timer.start();
            std::string message = std::to_string(my_id) + " is ok";
            send_message(heartbeat_socket, message);
        }
    }
    
private:
    zmq::context_t context;
    zmq::socket_t pull_socket;
    zmq::socket_t push_socket;
    zmq::socket_t heartbeat_socket;
    int my_id;
    int heartbeat_time;
};


int main(int argc, char* argv[]) { // id and parent port number
    if (argc != 6) {
        std::cout << "Usage: " << argv[0] << " id pull_port push_port heartbeat_port heartbeat_time\n";
        return 1;
    }
    int id = std::stoi(argv[1]);
    int pull_port = std::stoi(argv[2]);
    int push_port = std::stoi(argv[3]);
    int heartbeat_port = std::stoi(argv[4]);
    int heartbeat_time = std::stoi(argv[5]);

    Worker worker{id, pull_port, push_port, heartbeat_port, heartbeat_time};
    std::thread work(std::bind(&Worker::Work, &worker));
    std::thread heartbeat(std::bind(&Worker::Heartbeat, &worker));

    work.join();
    heartbeat.join();
    return 0;
}
