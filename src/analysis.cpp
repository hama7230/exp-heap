#include <iostream>
#include <fstream>
#include <vector>

#define MEM_SIZE 0x21000
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

vector<Step*> steps;



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
    ifstream ifs; 
    ifs.open(path); 

    while (getline(ifs, buf)) {
        void* ptr;
        size_t nmemb, size;
        if (buf.find("called") != string::npos) {
            if (buf.find("malloc") != string::npos) {
                getline(ifs, buf);
                sscanf(buf.c_str(), "size: %llx", (long long unsigned int*)&size);
                getline(ifs, buf);
                sscanf(buf.c_str(), "ptr: %llx", (long long unsigned int*)&ptr);
                
                cout << "malloc" << endl;
                cout << size << endl;
                cout << ptr << endl;
                Step *step = new Step(MALLOC, ptr, size, 0xdeadbeef);
                steps.push_back(step);
            }
            if (buf.find("free") != string::npos) {
                getline(ifs, buf);
                sscanf(buf.c_str(), "ptr: %llx", (long long unsigned int*)&ptr);
                    
                cout << "free" << endl;
                cout << ptr << endl;
            }
            if (buf.find("calloc") != string::npos) {
                getline(ifs, buf);
                sscanf(buf.c_str(), "nmemb: %llx", (long long unsigned int*)&nmemb);
                getline(ifs, buf);
                sscanf(buf.c_str(), "size: %llx", (long long unsigned int*)&size);
                getline(ifs, buf);
                sscanf(buf.c_str(), "ptr: %llx", (long long unsigned int*)&ptr);
            }
           
        }
    }
        
    ifs.close();
    

    
    return 0;
}

