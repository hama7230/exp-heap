#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <memory>


#define NOT_USED    0xdeadbeeffeedbabe

enum function{MALLOC, FREE, REALLOC};
using namespace std;


class Mem {
    public:
        static const size_t size = 0x21000;
        unsigned char memory[size];
};

static Mem heap;

// step-by-stepで解析していくときのchunk情報を持つクラス 
class Chunk {
    private:
        bool isUsed;
        void* addr;
        size_t prev_size;
        size_t size;
        void* fd;
        void* bk;
        void* fd_nextsize;
        void* bk_nextsize;
        
        //static const size_t SIZE_SZ = sizeof(size_t);
        static const size_t SIZE_SZ = 8;
        static const size_t MALLOC_ALIGN_MASK = 2*SIZE_SZ - 1;
        static const size_t MIN_CHUNK_SIZE = 0x20; 
        static const size_t MINSIZE = (unsigned long)(((MIN_CHUNK_SIZE+MALLOC_ALIGN_MASK) & ~MALLOC_ALIGN_MASK));
        
        size_t reqeuset2size(size_t req) {
            size_t result = (((req) + SIZE_SZ + MALLOC_ALIGN_MASK < MINSIZE)  ? MINSIZE : ((req) + SIZE_SZ + MALLOC_ALIGN_MASK) & ~MALLOC_ALIGN_MASK);
            return result;
            //return (((req) + SIZE_SZ + MALLOC_ALIGN_MASK < MINSIZE)  ? MINSIZE : ((req) + SIZE_SZ + MALLOC_ALIGN_MASK) & ~MALLOC_ALIGN_MASK);
        }
    public:
        Chunk(void* _addr, size_t _size)
            : isUsed(true)
            , addr((void*)((uint64_t)_addr-0x10))
            , prev_size(0)
            , size(reqeuset2size(_size))
            , fd(nullptr)
            , bk(nullptr)
            , fd_nextsize(nullptr)
            , bk_nextsize(nullptr)
        {
        }
        
        void* get_addr() const { return addr; }
        size_t get_size() const { return size; }
        bool isFree() const { return !isUsed; }
        void free(void) {
            cout << "free()..."  << endl;
            isUsed = false;
             
            
        }
        void reuse(void) {
            isUsed = true;
        }
};
// 仮想的なmain arena
class Arena {
        
    public:
       // Chunk fastbins;
        Arena() {
        

        }
};



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
        size_t get_size() const { return size; }
        size_t get_nmemb() const { return nmemb; }
        int get_function() const { return function; }
        
        string toString() const {
            char buf[0x100];
            switch(function) {
                case MALLOC:
                    if (nmemb == NOT_USED) 
                        snprintf(buf, sizeof(buf), "malloc %p %zd", ptr, size);
                    else
                        snprintf(buf, sizeof(buf), "calloc %p %zd", ptr, size*nmemb);
                    break;
                case FREE:
                    snprintf(buf, sizeof(buf), "free %p", ptr);
                    break;
                case REALLOC:
                    snprintf(buf, sizeof(buf), "realloc %p %zd", ptr,  size);
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
                ifs >> hex >> size >> ptr;
                steps.emplace_back(MALLOC, ptr, size, NOT_USED);
            }
            if (buf.find("calloc") != string::npos) {
                ifs >> hex >> size >> nmemb >> ptr;
                steps.emplace_back(MALLOC, ptr, size, nmemb);
            }
            if (buf.find("free") != string::npos) {
                ifs >> hex >> ptr; 
                steps.emplace_back(FREE, ptr, NOT_USED, NOT_USED);
            }
            if (buf.find("realloc") != string::npos) {
                ifs >> hex >> nmemb >> size >> ptr;
                steps.emplace_back(REALLOC, ptr, size, NOT_USED);
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
    for (size_t i=0; i < steps.size(); i++) {
        size_t j = 0;
        // vector<void*> chunks;
        vector<Chunk> chunks;
        for (const auto& s : steps) {
            if (i < j) 
                break;
            j++;
            
            // addrでsort
            sort(chunks.begin(), chunks.end(), [](const Chunk a, const Chunk b) {
                return a.get_addr() < b.get_addr();
            });


            if (s.get_function() == MALLOC) {
                //chunks.push_back(s.get_ptr());
                bool isNewchunk = true;
                size_t realsize;
                for (auto& chunk : chunks) {
                    // just-fit
                    realsize = s.get_nmemb() == NOT_USED ? s.get_size() : s.get_size()*s.get_nmemb();
                    if (chunk.isFree() && chunk.get_size() == realsize) {
                         chunk.reuse();
                         isNewchunk = false;
                         break;
                    }
                }
                if (isNewchunk)
                    chunks.emplace_back(s.get_ptr(), realsize);
            
            } else if (s.get_function() == REALLOC) {
            } else if (s.get_function() == FREE) {
                /*
                vector<void*>::iterator it = find(chunks.begin(), chunks.end(), s.get_ptr());
                if (it == chunks.end()) {
                    cout << "invalid pointer? or arbitary address free?" << endl;
                } else {
                    chunks.erase(it);
                }*/
                for (auto it = chunks.begin(); it != chunks.end(); it++) {
                }
                bool isExisted = false;
                for (auto& chunk : chunks) {
                    void* ptr = (void*)((uint64_t)chunk.get_addr() + 0x10);
                    if (ptr == s.get_ptr()) {
                        // chunkをfree()後の状態にする．
                        // arenaの更新などなど
                        chunk.free();
                        isExisted = true;
                        break;
                    }
                }
                if (!isExisted) {
                    cout << "invalid pointer? or arbitary address free?" << endl;
                }
            } else {
                cout << "unknown funtion" << endl;
            }
            
        }
        
        cout << "------------------------------" << endl;
        for (const auto& chunk: chunks) {
            if (!chunk.isFree()) 
                printf("%lx:%lx\n", (uint64_t)chunk.get_addr(),  (uint64_t)chunk.get_size());
        } 
        cout << "------------------------------" << endl;
    }
}


void print_steps(void) {
    for (const auto& s : steps) {
        cout << s.toString() << endl;
    }
}



