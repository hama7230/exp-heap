#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

enum function{MALLOC, FREE, REALLOC};
using namespace std;


class Mem {
    public:
        static const size_t size = 0x21000;
        unsigned char memory[size];
};

static Mem heap;


// heapに対する操作を記憶するクラス
class Step {
    private:
        int function;
        bool isUsed;
        void* ptr;
        size_t size;
        size_t nmemb;
    public:
        Step(int _function, void* _ptr, size_t _size, size_t _nmemb)
            : function(_function)
            , isUsed(true)
            , ptr(_ptr)
            , size(_size)
            , nmemb(_nmemb)
        {
        }
        void* get_ptr() const { return ptr; }
        size_t get_size() const { return size;}
        size_t get_nmemb() const { return nmemb; }
        int get_function() const { return function; }
        
        string toString() const {
            char buf[0x100];
            switch(function) {
                case MALLOC:
                    snprintf(buf, sizeof(buf), "malloc %p %zd", ptr, size);
                    break;
                case FREE:
                    snprintf(buf, sizeof(buf), "free %p", ptr);
                    break;
                case REALLOC:
                    snprintf(buf, sizeof(buf), "realloc %p %zd %zd", ptr, nmemb, size);
                    break;
                default:
                    break;
            }
            return buf;
        }
};
vector<Step> steps;



void step_by_step(void);
void print_steps(void);

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: filename" << endl;
        exit(1);
    }
    string path = "/tmp/trace_heap/";
    path = path + argv[1];
    
    // load logfile
    string buf;
    ifstream ifs(path);    
    while (getline(ifs, buf)) {
        void* ptr;
        size_t nmemb, size;
        if (buf.find("called") != string::npos) {
            if (buf.find("malloc") != string::npos) {
                ifs >> size >> ptr;
                steps.emplace_back(MALLOC, ptr, size, 0xdeadbeef);
            }
            if (buf.find("free") != string::npos) {
                ifs >> ptr; 
                steps.emplace_back(FREE, ptr, 0xdeadbeef, 0xdeadbeef);
            }
            if (buf.find("calloc") != string::npos) {
                ifs >> nmemb >> size >> ptr;
                steps.emplace_back(REALLOC, ptr, size, nmemb);
            }
           
        }
    }
    
    
    // load memory dump
    path = path + ".dump";
    ifstream ifs_dump(path,  ios::binary);
    if (!ifs_dump) {
        cout << "dump file open error" << endl;
        return 1;
    }
    if (!ifs_dump.is_open()) {
        cout << "dump file open error" << endl;
        return 1;
    }

    ifs_dump.seekg(0, ios::end);
    size_t size = ifs_dump.tellg();
    ifs_dump.seekg(0, ios::beg);
    ifs_dump.read((char*)heap.memory, size);

    
    while(1) {
        cout << "================================\n1. print steps\n2. print step by step\n3. fugafuga\n" << endl;
        int choice;
        cin >> choice;
        switch (choice) {
            case 1:
                print_steps();
                break;
            case 2:
                step_by_step(); 
                break;
        }
    }



    return 0;

}


void step_by_step() {
    for (int i=0; i < steps.size(); i++) {
        int j = 0;
        vector<void*> chunks;
        for (const auto& s : steps) {
            if (i < j) 
                break;
            j++;
            
            if (s.get_function() == MALLOC || s.get_function() == REALLOC) {
                chunks.push_back(s.get_ptr());
            } else if (s.get_function() == FREE) {
                vector<void*>::iterator it = find(chunks.begin(), chunks.end(), s.get_ptr());
                if (it == chunks.end()) {
                    cout << "invalid pointer? or arbitary address free?" << endl;
                } else {
                    chunks.erase(it);
                }
            } else {
                cout << "unknown funtion" << endl;
            }
            
        }
        // finaly log
        cout << "------------------------------" << endl;
        for (const auto& chunk: chunks) {
            cout << chunk << endl;
        } 
        cout << "------------------------------" << endl;
    }
}


void print_steps(void) {
    for (const auto& s : steps) {
        cout << s.toString() << endl;
    }
}



