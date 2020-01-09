#include <iostream>
#include <string>
#include <map>
#include <set>
#include <thread>
#include <memory>
#include <functional>
#include <signal.h>

#include "zmq.hpp"
#include "functions.hpp"
#include "timer.hpp"

#define HEARTBEAT_LIVES 4
#define TIMEOUT 200

struct Server {
public:
    Server(int port) :
        context{1},
        main_socket{context, ZMQ_PULL},
        pull_worker{context, ZMQ_PULL},
        heartbeat_worker{context, ZMQ_PULL},
        my_port(port),
        heartbeat_time(0)
    {
        my_port = port;
        try {
            main_socket.bind(get_port_name(port));
        } catch(...) {
            std::cout << "Port is unavailable" << std::endl;
            my_port = bind_socket(main_socket);
            std::cout << "Your port is " << my_port << std::endl;
        }
        int linger = 0;
        pull_worker_port = bind_socket(pull_worker);
        heartbeat_worker.setsockopt(ZMQ_RCVTIMEO, TIMEOUT);
        heartbeat_worker.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
        heartbeat_worker_port = bind_socket(heartbeat_worker);
    }

    void Reciever () {
        while (true) {
            zmq::message_t message;
            bool ok;
            ok = pull_worker.recv(&message);
                
            std::string result = msg_to_string(message);
            std::istringstream is(result);
            std::string what_is;
            is >> what_is;
            if (what_is == "port") {
                int port;
                is >> port;
                std::string res_mes;
                while (is) {
                    std::string s;
                    is >> s;
                    res_mes += s + " ";
                }
                res_mes = "recv " + res_mes;
                send_message(neighbours[port], res_mes);
            }
            else {
                std::cout << result << std::endl;
            }
        }
    }

    void Heartbeater() {
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
            for (auto& worker : push_worker) {
                if (push_lives[worker.first].first > -5)
                    --push_lives[worker.first].first;
                bool ok;
                zmq::message_t message;
                try {
                    ok = heartbeat_worker.recv(&message);   
                } catch (...) {
                    ok = false;
                }
                if (!ok) {
                    continue;
                }
                std::string heartbeat_string = msg_to_string(message);
                std::istringstream heartbeat_stream(heartbeat_string);
                std::string str_id;
                heartbeat_stream >> str_id;
                push_lives[std::stoi(str_id)].first = HEARTBEAT_LIVES;
            }
            delete_zero_map(); // mark dead workers from both maps
        }
    }

private:
    void delete_zero_map() {
        std::map<int, std::pair<int, int>>::iterator live_cur, live_end;
        live_cur = push_lives.begin();
        live_end = push_lives.end();
        while (live_cur != live_end) {
            if (live_cur->second.first == 0){
                live_cur++;
            }
            else
                ++live_cur;
        }
    }
public:
    
    void Sender () {
        while (true) {
            std::string cmd;
            std::cin >> cmd;
            if (cmd == "create" || cmd == "remove" || cmd == "exec") {
                int id;
                try {
                    std::cin >> id;
                } catch(...) {
                    std::cout << "Wrong command" << std::endl;
                    continue;
                }
                bool worker_here = (push_worker.find(id) != push_worker.end());
                std::pair<bool, int> checked = CheckNeibs(id);

                // запросы к своим воркерам
                if (worker_here && cmd == "create") {
                    std::cout << "Error: Already exists" << std::endl;
                }
                else if (worker_here && cmd == "remove") {
                    RemoveNode(id, "", std::cout);
                }
                else if (worker_here && cmd == "exec") {
                    Exec(std::cin, id, std::cout, "");
                }

                // запросы к соседу
                else if (checked.first && cmd == "create") {
                    std::cout << "Error: Already exists" << std::endl;
                }
                else if (checked.first && cmd == "remove") {
                    std::string send_mes = "port " + std::to_string(my_port) +
                        " send remove " + std::to_string(id);
                    if (!send_message(neighbours[checked.second], send_mes)) {
                        std::cout << "Server is unavailable" << std::endl;
                    }
                }
                else if (checked.first && cmd == "exec") {
                    std::string type;
                    std::cin >> type;
                    std::string send_mes = "port " + std::to_string(my_port) +
                        " send exec " + std::to_string(id) + " " + type;
                    if (!send_message(neighbours[checked.second], send_mes)) {
                        std::cout << "Server is unavailable" << std::endl;
                    }
                }

                // некуда запрашивать
                else if (cmd == "create") {
                    CreateNode(id, std::cout);
                }
                else if (cmd == "remove") {
                    std::cout << "Error:" << id << ": Not found" << std::endl;
                }
                else if (cmd == "exec") {
                    std::cout << "Error:" << id << ": Not found" << std::endl;
                }
            }
            
            else if (cmd == "union") {
                Union(std::cin, std::cout);
            }
            else if (cmd == "heartbeat") {
                Heartbeat(std::cin, std::cout);
            }
            else {
                std::cout << "Wrong command" << std::endl;
            }
        }
    }

private:
    // Проверить, нет ли такой ноды у соседей
    std::pair<bool, int> CheckNeibs(int id) {
        int where_is = -1;
        int sent = false;
        for (auto& neib : neibWorkers) {
            if (neib.second.find(id) == neib.second.end())
                continue;
            where_is = neib.first;
            sent = true;
            break;
        }
        return std::pair{sent, where_is};
    }

    void CreateNode(int id, std::ostream& os) {
        int linger = 0;
        push_worker[id] = zmq::socket_t(context, ZMQ_PUSH);
        push_worker[id].setsockopt(ZMQ_RCVTIMEO, TIMEOUT);
        push_worker[id].setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
        int port = bind_socket(push_worker[id]);

        int pid = fork();
        if (pid == -1) {
            os << "Error: Unable to create worker" << std::endl;
            pid = 0;
            push_worker.erase(id);
            return ;
        }
        else if (pid == 0) { 
            create_worker(id, port, pull_worker_port, heartbeat_worker_port, heartbeat_time);
        }
        else { 
            push_lives[id].first = HEARTBEAT_LIVES;
            push_lives[id].second = pid;
            send_message(push_worker.at(id), "pid");
        }

        // рассказать о новорожденном
        std::string message = "send added " + std::to_string(my_port) + " " + std::to_string(id);
        for (auto& neib : neighbours) {
            send_message(neib.second, message);
        }
    }
    
    bool RemoveNode(int id, std::string prefix, std::ostream& os) {
        if (push_lives[id].first <= 0) {
            os << "Error: Node is unavailable" << std::endl;
            return false;
        }
        if (push_worker.find(id) == push_worker.end()) {
            os << "Error:" << id << ": Not found" << std::endl;
            return false;
        }
        kill(push_lives[id].second, SIGTERM);
        kill(push_lives[id].second, SIGKILL);
        push_lives.erase(id);
        push_worker.erase(id);
        
        // рассказать об умершем
        std::string message = "send deleted " + std::to_string(my_port) + " " + std::to_string(id);
        for (auto& neib : neighbours) {
            send_message(neib.second, message);
        }
        
        if (prefix.size() != 0) {
            std::istringstream is(prefix);
            std::string str;
            int port;
            is >> str >> port;
            send_message(neighbours[port], "Ok.");
        }
        else {
            os << "Ok." << std::endl;
        }
    }

    bool Exec(std::istream& is, int id, std::ostream& os, const std::string& prefix) {
        if (push_worker.find(id) == push_worker.end()) {
            os << "Error:" << id << ": Not found" << std::endl;
            return false;
        }
        if (push_lives[id].first <= 0) {
            os << "Error: Node is unavailable" << std::endl;
            return false;
        }
        std::string type;
        is >> type;
        std::string msg_string = prefix + "exec " + std::to_string(id) + " " + type;
        
        bool sent = send_message(push_worker.at(id), msg_string);
        return sent;
    }

    void Union(std::istream& is, std::ostream& os) {
        int port;
        is >> port;
        if (port == my_port) {
            std::cout << "Error: Other server port and the current port "
                      << my_port << " is equal" << std::endl;
            return;
        }
        if (neighbours.find(port) != neighbours.end()) {
            std::cout << "We are already connected" << std::endl;
            return;
        }

        std::string send_union = "send union " + std::to_string(port);
        for (auto& neib : neighbours) {
            send_message(neib.second, send_union);
        }
        
        // сказать о своих воркерах
        std::string join_message = "join " + std::to_string(my_port);
        for (auto& worker : push_worker) {
            join_message += " " + std::to_string(worker.first);
        }

        int linger = 0;
        neighbours[port] = zmq::socket_t(context, ZMQ_PUSH);
        neighbours[port].connect(get_port_name(port));
        neighbours[port].setsockopt(ZMQ_SNDTIMEO, TIMEOUT);
        neighbours[port].setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
                
        bool ok = send_message(neighbours[port], join_message);
        if (!ok) {
            os << "Error: Message didn't send" << std::endl;
            return;
        }
    }

    void Heartbeat(std::istream& is, std::ostream& os) {
        int time; 
        is >> time;
        heartbeat_time = time;
        std::string message = "heartbeat " + std::to_string(time);
        for (auto& worker : push_worker) {
            send_message(worker.second, message);
        }
        os << "Ok." << std::endl;
    }
    
public:
    void Unionist() {
        while (true) {
            zmq::message_t message;
            while (true) {
                try {
                    main_socket.recv(&message);
                } catch (...) { continue; }
                break;
            }

            std::stringstream is(msg_to_string(message));
            std::string prefix = get_prefix(is);
            std::string cmd;
            is >> cmd;

            if (cmd.substr(0, 2) == "Ok") {
                std::cout << cmd << std::endl;
            }
            else if (cmd == "join") {
                int port;
                is >> port;
                
                int linger = 0;
                neighbours[port] = zmq::socket_t(context, ZMQ_PUSH);
                neighbours[port].connect(get_port_name(port));
                neighbours[port].setsockopt(ZMQ_SNDTIMEO, TIMEOUT);
                neighbours[port].setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

                neibWorkers[port].clear();
                while (is) {
                    int id;
                    is >> id;
                    neibWorkers[port].insert(id);
                    if (push_worker.find(id) != push_worker.end())
                        RemoveNode(id, prefix, std::cout);
                }

                std::string send_union = "send union ";
                for (auto& neib : neighbours) {
                    send_union += std::to_string(neib.first);
                    send_message(neighbours[port], send_union);
                }
                
                std::string ans_mes = "send added " + std::to_string(my_port);
                for (auto& worker : push_worker) {
                    ans_mes += " " + std::to_string(worker.first);
                }
                
                send_message(neighbours[port], ans_mes);
                if (!send_message(neighbours[port], "Ok."))
                    std::cout << "He is dead" << std::endl;;
            }
            
            else if (cmd == "send") {
                UnionistSend(is, prefix);
            }
            else if (cmd == "recv") {
                UnionistRecv(is, prefix);
            }
        }
    }

private:
    // получатель - рабочий
    void UnionistSend(std::istream& is, const std::string& prefix) {
        std::string cmd;
        is >> cmd;
        if (cmd == "exec") {
            int id;
            is >> id;
            Exec(is, id, std::cout, prefix);
        }
        else if (cmd == "remove") {
            int id;
            is >> id;
            RemoveNode(id, prefix, std::cout);
        }
        else if (cmd == "added") {
            int port;
            is >> port;
            neibWorkers[port].clear();
            while (is) {
                int id;
                is >> id;
                neibWorkers[port].insert(id);
            }
        }
        else if (cmd == "deleted") {
            int port;
            is >> port;
            while (is) {
                int id;
                is >> id;
                neibWorkers[port].erase(id);
            }
        }
        else if (cmd == "union") {
            int port;
            is >> port;
            if (port == my_port || neighbours.find(port) != neighbours.end()) {
                return;
            }
            // сказать о своих воркерах
            std::string join_message = "join " + std::to_string(my_port);
            for (auto& worker : push_worker) {
                join_message += " " + std::to_string(worker.first);
            }

            int linger = 0;
            neighbours[port] = zmq::socket_t(context, ZMQ_PUSH);
            neighbours[port].setsockopt(ZMQ_SNDTIMEO, TIMEOUT);
            neighbours[port].setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
            neighbours[port].connect(get_port_name(port));
                
            send_message(neighbours[port], join_message);
        }
    }

    // получатель заказчик
    void UnionistRecv(std::istream& is, const std::string& prefix) { 
        if (prefix.size() == 0) {
            std::cout << is.rdbuf() << std::endl;
        }
        else {
            
            std::string what_is;
            is >> what_is;
            int port;
            is >> port;
            std::string sstr(std::istreambuf_iterator<char>(is), {});
            std::string res_mes = "recv " + sstr; 
            send_message(neighbours[port], res_mes);
        }
    }
        
private:
    zmq::context_t context;
    zmq::socket_t main_socket;
    zmq::socket_t pull_worker;
    zmq::socket_t heartbeat_worker;
    int my_port;
    int heartbeat_time;
    int pull_worker_port;
    int heartbeat_worker_port;
    std::map<int, zmq::socket_t> neighbours;
    std::map<int, std::set<int>> neibWorkers;
    std::map<int, zmq::socket_t> push_worker;
    std::map<int, std::pair<int, int>> push_lives; // колво хп и пид воркера
};
    

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <port> " << std::endl;
        return 1;
    }
    int port = std::stoi(argv[1]);
    
    Server server{port};
    
    std::thread sender(std::bind(&Server::Sender, &server));
    std::thread reciever(std::bind(&Server::Reciever, &server));
    std::thread heartbeat(std::bind(&Server::Heartbeater, &server));
    std::thread main_socket(std::bind(&Server::Unionist, &server));
    
    sender.join();
    reciever.join();
    heartbeat.join();
    main_socket.join();
    return 0;
}
