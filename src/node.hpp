#pragma once

#include <iostream>

class Pos {
    int x;
    int y;

    Pos(int x, int y) : x(x), y(y) {}
};

class Nodes;

class Node {

    Nodes* link = nullptr;

    public:

    Node(Nodes* link = nullptr) {
        this->link = link;
    };
};

class Nodes {
    Node* data = nullptr;
    int n = 0;
    
    public:

    Nodes(int n, Node* other) {
        if (n <= 0) {
            std::cout << "Warning: invalid size" << std::endl;
        } else if (!data) {
            std::cout << "Warning: data is null" << std::endl;
        } else {
            this->n = n;
            this->data = other;
        }
    }

    Nodes(int n) : n(n) {
        if (n > 0) {
            data = new Node[n];
            if (data) {
                
            }
        }
    }

    ~Nodes() {
        if (data) {
            delete data;
        }
    }
};
