#include <iostream>
#include <fstream>
#include <vector>

enum function{MALLOC, FREE, REALLOC};
using namespace std;


class Mem {
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
    for (const auto& s : steps) {
        cout << s.toString() << endl;
    }
}


