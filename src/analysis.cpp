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
        
        static const size_t SIZE_SZ = sizeof(size_t);
        static const size_t MALLOC_ALIGN_MASK = 2*SIZE_SZ - 1;
        static const size_t MIN_CHUNK_SIZE = 0x20; 
        static const size_t MINSIZE = (unsigned long)(((MIN_CHUNK_SIZE+MALLOC_ALIGN_MASK) & ~MALLOC_ALIGN_MASK));
        
        static const size_t global_max_fast = 0x80;

        size_t reqeuset2size(size_t req) {
            size_t result = (((req) + SIZE_SZ + MALLOC_ALIGN_MASK < MINSIZE)  ? MINSIZE : ((req) + SIZE_SZ + MALLOC_ALIGN_MASK) & ~MALLOC_ALIGN_MASK);
            return result;
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
            
            // for fastbin
            if (size <= global_max_fast) {
                uint32_t idx_fb = (size - MIN_CHUNK_SIZE) / 0x10;
                fd = nullptr; // fastbinにchunkがある場合は，linked listにする
                bk = nullptr;
            }
            
        }
        void reuse(void) {
            isUsed = true;
        }
};
// 仮想的なmain arena
class Arena {
        
    public:
       // Chunk fastbins[7];
       // Chunk unsortedbin;
       // Chunk smallbins[127];
       // Chunk largebins[127];
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


class MemoryHistory {
    std::vector<Step> steps;
    public:
       void loadLog(const std::string fileName);
       void loadDump(const std::string fileName);
       void printSteps() const;
       void printStepByStep() const;
};
MemoryHistory memoryHistory;


void MemoryHistory::loadLog(const std::string fileName) {
    // load logfile
    string buf;
    ifstream ifs(fileName);    
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
}

void MemoryHistory::loadDump(const std::string fileName) {
    // load memory dump
    std::string path = fileName + ".dump";
    ifstream ifs_dump(path,  ios::binary);
    if (!ifs_dump) {
        cout << "dump file open error" << endl;
        exit(1);
    }
    if (!ifs_dump.is_open()) {
        cout << "dump file open error" << endl;
        exit(1);
    }

    ifs_dump.seekg(0, ios::end);
    size_t size = ifs_dump.tellg();
    ifs_dump.seekg(0, ios::beg);
    ifs_dump.read((char*)heap.memory, size);
}

void MemoryHistory::printSteps() const {
    for (const auto& s : steps) {
        cout << s.toString() << endl;
    }
}

void MemoryHistory::printStepByStep() const {
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



int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: filename" << endl;
        exit(1);
    }
    string path = "/tmp/trace_heap/";
    path = path + argv[1];
    
    memoryHistory.loadLog(path);
    memoryHistory.loadDump(path);

    while(1) {
        cout << "================================\n1. print steps\n2. print step by step\n3. fugafuga\n" << endl;
        int choice;
        cin >> choice;
        switch (choice) {
            case 1:
                memoryHistory.printSteps();
                break;
            case 2:
                memoryHistory.printStepByStep();
                break;
        }
    }
    return 0;
}


