#include <iostream>
#include <fstream>
#include <vector>

const size_t MEM_SIZE = 0x21000;
enum function{MALLOC, FREE, REALLOC};
using namespace std;


class Mem {
    public:
        
    private:
        unsigned char memory[MEM_SIZE];
};
static Mem heap;


// heapに対する操作を記憶するクラス
class Step {
    public:
        Step(int _function, void* _ptr, size_t _size, size_t _nmemb) {
            function = _function;
            ptr = _ptr;
            size = _size;
            nmemb = _nmemb;
            used_flag = true; 
        }
        void* get_ptr() { return ptr; }
        size_t get_size() { return size;}
        size_t get_nmemb() { return nmemb; }
        int get_function() { return function; }
        
    private:
        void* ptr;
        size_t size;
        size_t nmemb;
        int function;
        bool used_flag;
};
vector<Step> steps;


void print_steps(void);

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: filename" << endl;
        exit(1);
    }
    cout << argv[1] << endl;
    string path = "/tmp/trace_heap/";
    path = path + argv[1];
    cout << path << endl;
    
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
    
    print_steps();
    
    return 0;
}



void print_steps(void) {
    for (auto& s : steps) {
        switch (s.get_function()) {
            case MALLOC:
                cout << "malloc ";
                cout << s.get_ptr() << " " << s.get_size() << endl; 
                break;
            case FREE:
                cout << "free ";
                cout << s.get_ptr() << endl;
                break;
            case REALLOC:
                cout << "realloc ";
                cout << s.get_nmemb() << " " << s.get_ptr() << " " << s.get_size() << endl; 
                break;
        };
    }
}


