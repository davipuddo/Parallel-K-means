#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

class Pos {

    public:

    double x;
    double y;

    Pos(double x=0, double y=0) : x(x), y(y) {}

    ~Pos() = default;

    void operator+=(Pos& other) {
        x = x + other.x;
        y = y + other.y;
    }

    void operator+=(double d) {
        x = x + d;
        y = y + d;
    }

    Pos operator+(double d) {
        return Pos(x+d, y+d);
    }

    Pos operator/(double div) {
        return Pos(x/div, y/div);
    }

    double distance (Pos other) const {
        double dx = (x - other.x);
        double dy = (y - other.y);
        return (dx * dx) + (dy * dy);
    }
    
};

class Node {

    public:

    Pos pos;

    Node(Pos pos = Pos(0, 0)): pos(pos) {}

    Node(double x, double y): pos(Pos(x, y)) {}

    ~Node() = default;

    bool operator==(Node other) {
        return ((other.pos.x == pos.x) && (other.pos.y == pos.y));
    }

    double distance (Node other) const {
        return pos.distance(other.pos);
    }

    std::string to_string() {
        std::string x = std::to_string(pos.x);
        std::string y = std::to_string(pos.y);
        return "(" + x + ", " + y + ")";
    }
};

class Nodes {

    std::vector<Node> data;
    
    public:

    Nodes(int n=0) : data(std::vector<Node>(n)) {}

    Nodes(const char* path) {
        std::ifstream file (path);

        if (!file.is_open()) {
            std::cerr << "Error: Could not open the file: \"" << path << "\"" << std::endl;
        } else {
            std::string s;

            while (std::getline(file, s)) {
                std::stringstream ss (s);

                std::string word;

                ss >> word;
                double x = std::atof(word.c_str());

                ss >> word;
                double y = std::atof(word.c_str());

                Node n = Node(x, y);

                this->add(n);
            }
        }
    }

    ~Nodes() = default;

    void add (Node x) {
        data.push_back(x);
    }

    const Node& operator[](const int i) const {
        return data[i];
    }

    Node& operator[](const int i) {
        return data[i];
    }

    ulong size() const {
        return data.size();
    }

    void print() {
        for (Node n : data) {
            std::string s = n.to_string();
            std::cout << s << ", ";
        }
        std::cout << std::endl;
    }
};
