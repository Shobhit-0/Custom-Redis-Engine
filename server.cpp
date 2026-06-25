#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <queue>
#include <deque>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>
#include <ctime>


// CUSTOM TREAP ENGINE (Key-Value & TTL)
struct Node {
    std::string key;
    std::string value;
    long long expire_at; 
    int priority;
    Node* left;
    Node* right;

    Node(std::string k, std::string v, long long exp) {
        key = k; value = v; expire_at = exp; priority = rand() % 10000;
        left = nullptr; right = nullptr;
    }
};

class Treap {
private:
    Node* root;
    Node* rightRotate(Node* y) { Node* x = y->left; y->left = x->right; x->right = y; return x; }
    Node* leftRotate(Node* x) { Node* y = x->right; x->right = y->left; y->left = x; return y; }

    Node* insertNode(Node* node, std::string key, std::string value, long long exp) {
        if (!node) return new Node(key, value, exp);
        if (key < node->key) {
            node->left = insertNode(node->left, key, value, exp);
            if (node->left->priority > node->priority) node = rightRotate(node);
        } else if (key > node->key) {
            node->right = insertNode(node->right, key, value, exp);
            if (node->right->priority > node->priority) node = leftRotate(node);
        } else {
            node->value = value; node->expire_at = exp;
        }
        return node;
    }

    Node* deleteNode(Node* root, std::string key) {
        if (!root) return nullptr;
        if (key < root->key) root->left = deleteNode(root->left, key);
        else if (key > root->key) root->right = deleteNode(root->right, key);
        else {
            if (!root->left) { Node* temp = root->right; delete root; return temp; }
            else if (!root->right) { Node* temp = root->left; delete root; return temp; }
            else {
                if (root->left->priority > root->right->priority) {
                    root = rightRotate(root); root->right = deleteNode(root->right, key);
                } else {
                    root = leftRotate(root); root->left = deleteNode(root->left, key);
                }
            }
        }
        return root;
    }

public:
    Treap() { root = nullptr; srand(time(0)); }
    void set(std::string key, std::string value, long long exp) { root = insertNode(root, key, value, exp); }
    void remove(std::string key) { root = deleteNode(root, key); }
    std::string get(std::string key) {
        Node* curr = root;
        while (curr != nullptr) {
            if (key == curr->key) {
                if (curr->expire_at > 0 && time(NULL) >= curr->expire_at) {
                    remove(key); return "(nil)";
                }
                return curr->value;
            }
            if (key < curr->key) curr = curr->left;
            else curr = curr->right;
        }
        return "(nil)";
    }
};


// GLOBAL DATABASES & AOF PERSISTENCE
Treap kv_store;
std::map<std::string, std::deque<std::string>> queue_store; 
std::ofstream aof_writer;

// Log operations that change data to disk instantly
void append_to_aof(const std::string& command) {
    aof_writer << command << "\n";
    aof_writer.flush(); // Force write to hard drive immediately
}


// 3. COMMAND DISPATCHER
// This handles all logic, making the network code clean
std::string execute_command(const std::string& raw_message, bool is_replaying = false) {
    std::stringstream ss(raw_message);
    std::string command, key, value;
    ss >> command;

    if (command == "SET") {
        ss >> key >> value;
        long long ttl = 0; 
        if (ss >> ttl) ttl += time(NULL); 
        kv_store.set(key, value, ttl);
        
        if (!is_replaying) append_to_aof(raw_message);
        return "OK\n";
    } 
    else if (command == "GET") {
        ss >> key;
        return kv_store.get(key) + "\n";
    } 
    else if (command == "DEL") {
        ss >> key;
        kv_store.remove(key);
        if (!is_replaying) append_to_aof(raw_message);
        return "Deleted\n";
    }
    else if (command == "LPUSH") {
        ss >> key >> value;
        queue_store[key].push_front(value);
        if (!is_replaying) append_to_aof(raw_message);
        return "Queued: " + std::to_string(queue_store[key].size()) + "\n";
    }
    else if (command == "RPOP") {
        ss >> key;
        if (queue_store[key].empty()) return "(nil)\n";
        std::string res = queue_store[key].back();
        queue_store[key].pop_back();
        if (!is_replaying) append_to_aof(raw_message);
        return res + "\n";
    }
    return "Error: Unknown Command\n";
}

// Read the log file on startup and rebuild the database
void replay_aof() {
    std::ifstream aof_reader("database.aof");
    std::string line;
    int count = 0;
    while (std::getline(aof_reader, line)) {
        if (!line.empty()) {
            execute_command(line, true); 
            count++;
        }
    }
    std::cout << "[AOF] Replayed " << count << " commands to restore database state.\n";
}


// 4. NETWORK HELPERS
bool read_full(int fd, char* buf, int n) {
    int total = 0;
    while (total < n) {
        int rv = read(fd, buf + total, n - total);
        if (rv <= 0) return false;
        total += rv;
    }
    return true;
}

bool write_all(int fd, const char* buf, int n) {
    int total = 0;
    while (total < n) {
        int rv = write(fd, buf + total, n - total);
        if (rv <= 0) return false;
        total += rv;
    }
    return true;
}


// 5. EVENT LOOP (MAIN)
int main() {
    replay_aof();
    
    // Open the AOF file in Append Mode
    aof_writer.open("database.aof", std::ios_base::app);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(1234);
    address.sin_addr.s_addr = INADDR_ANY;
    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, SOMAXCONN);

    std::vector<struct pollfd> connections;
    connections.push_back({server_fd, POLLIN, 0});

    std::cout << "Advanced Engine Running on Port 1234...\n";

    while (true) {
        poll(connections.data(), connections.size(), -1); 

        for (int i = 0; i < connections.size(); i++) {
            if (!(connections[i].revents & POLLIN)) continue; 

            int current_fd = connections[i].fd;

            if (current_fd == server_fd) {
                struct sockaddr_in client_addr = {};
                socklen_t client_len = sizeof(client_addr);
                int new_client = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
                connections.push_back({new_client, POLLIN, 0});
            } 
            else {
                uint32_t len = 0;
                if (!read_full(current_fd, (char*)&len, 4)) {
                    close(current_fd);
                    connections.erase(connections.begin() + i);
                    i--; 
                    continue;
                }

                char buffer[4096] = {0};
                if (!read_full(current_fd, buffer, len)) continue;

                std::string message(buffer, len);
                std::string reply = execute_command(message);

                uint32_t reply_len = reply.length();
                write_all(current_fd, (char*)&reply_len, 4);
                write_all(current_fd, reply.c_str(), reply_len);
            }
        }
    }
    return 0;
}
