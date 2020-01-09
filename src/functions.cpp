#include "functions.hpp"
#include <cstring>

bool send_message(zmq::socket_t& socket, const std::string& message_string) {
    zmq::message_t message(message_string.size());
    memcpy(message.data(), message_string.c_str(), message_string.size());
    bool ok = false;
    try {
        ok = socket.send(message);
    }catch(...) {ok = false;}
    return ok;
}

std::string msg_to_string(zmq::message_t& message) {
    return std::string(static_cast<char*>(message.data()), message.size());
}

std::string recieve_message(zmq::socket_t& socket) {
    zmq::message_t message;
    bool ok;
    try {
        ok = socket.recv(&message);
    } catch (...) {
        ok = false;
    }
    std::string recieved_message(static_cast<char*>(message.data()), message.size());
    if (recieved_message.empty() || !ok) {
        return "Error: Worker is not available";
    }
    return recieved_message;
}

std::string get_port_name(int port) {
    return "tcp://127.0.0.1:" + std::to_string(port);
}

int bind_socket(zmq::socket_t& socket) {
    int port = 30000;
    while (true) {
        try {
            socket.bind(get_port_name(port));
            break;
        } catch(...) {
            port++;
        }
    }
    return port;
}

void create_worker(int id, int pull_port, int push_port, int heartbeat_port, int heartbeat) {
    char* arg1 = strdup((std::to_string(id)).c_str());
    char* arg2 = strdup((std::to_string(pull_port)).c_str());
    char* arg3 = strdup((std::to_string(push_port)).c_str());
    char* arg4 = strdup((std::to_string(heartbeat_port)).c_str());
    char* arg5 = strdup((std::to_string(heartbeat)).c_str());
    char* args[] = {"./worker", arg1, arg2, arg3, arg4, arg5,  NULL};
    execv("./worker", args);
}

std::string get_prefix(std::istream& is) {
    std::string res;
    while (true) {
        char c = ' ';
        while (c == ' ')
            c = is.get();
        is.unget();
        if (c != 'p') {
            break;
        }
        std::string prt;
        std::string portNumber;
        is >> prt >> portNumber;
        res = res + "port " +  portNumber + " ";
    }
    return res;
}
