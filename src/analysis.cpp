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

    private:
        void* ptr;
        size_t size;
        size_t nmemb;
        int function;
        bool used_flag;
};
vector<Step> steps;


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
                cout << "malloc" << endl;
                cout << size << endl;
                cout << ptr << endl;
                steps.emplace_back(MALLOC, ptr, size, 0xdeadbeef);
            }
            if (buf.find("free") != string::npos) {
                ifs >> ptr; 
                cout << "free" << endl;
                cout << ptr << endl;
                steps.emplace_back(FREE, ptr, 0xdeadbeef, 0xdeadbeef);
            }
            if (buf.find("calloc") != string::npos) {
                ifs >> nmemb >> size >> ptr;
                steps.emplace_back(REALLOC, ptr, size, nmemb);
            }
           
        }
    }
        
    ifs.close();
    

    
    return 0;
}

