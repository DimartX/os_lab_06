#pragma once
#include <string>
#include <zconf.h>
#include "zmq.hpp"

bool send_message(zmq::socket_t& socket, const std::string& message_string);

std::string recieve_message(zmq::socket_t& socket);

std::string get_port_name(int port);

int bind_socket(zmq::socket_t& socket);

void create_worker(int id, int pull_port, int push_port, int heartbeat_port, int heartbeat);

std::string msg_to_string(zmq::message_t& message);

std::string get_prefix(std::istream& is);
