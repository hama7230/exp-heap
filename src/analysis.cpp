#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <memory>

#define NOT_USED    0xdeadbeeffeedbabe

enum function{MALLOC, FREE, REALLOC};
using namespace std;

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
        Chunk* fd_ch;
        Chunk* bk_ch;
    public:
        static const size_t SIZE_SZ = sizeof(size_t);
        static const size_t MALLOC_ALIGN_MASK = 2*SIZE_SZ - 1;
        static const size_t MIN_CHUNK_SIZE = 0x20; 
        static const size_t MINSIZE = (unsigned long)(((MIN_CHUNK_SIZE+MALLOC_ALIGN_MASK) & ~MALLOC_ALIGN_MASK));
        
        static const size_t global_max_fast = 0x80;

        static size_t reqeuset2size(size_t req) {
            size_t result = (((req) + SIZE_SZ + MALLOC_ALIGN_MASK < MINSIZE)  ? MINSIZE : ((req) + SIZE_SZ + MALLOC_ALIGN_MASK) & ~MALLOC_ALIGN_MASK);
            return result;
        }
        Chunk(void* _addr, size_t _size)
            : isUsed(true)
            , addr((void*)((uint64_t)_addr-0x10))
            , prev_size(0)
            , size(reqeuset2size(_size))
            , fd(nullptr)
            , bk(nullptr)
            , fd_nextsize(nullptr)
            , bk_nextsize(nullptr)
            , fd_ch(nullptr)
            , bk_ch(nullptr)
        {
        }
        Chunk() {
         
        } 
        void* get_addr() const { return addr; }
        void* get_ptr() const { return (void*)((uint64_t)addr + 0x10); }
        size_t get_size() const { return size; }
        bool isFree() const { return !isUsed; }
        void* get_fd() const { return fd;}
        void* get_bk() const { return bk;}
        void set_fd(void* ptr) { fd = ptr; }
        void set_bk(void* ptr) { bk = ptr; }
        void set_fd_ch(Chunk* ch) { fd_ch = ch; }
        void set_bk_ch(Chunk* ch) { bk_ch = ch; }
        void set_isUsed(bool _isUsed) { isUsed = _isUsed; }
        Chunk* get_next() const { return fd_ch; }
        Chunk* get_prev() const { return bk_ch; }
        bool operator<(const Chunk& rhs) const {return addr < rhs.addr;}
};


// 仮想的なmain arena
class Arena {
    
    public:
       static const int size_fastbins = 7;
       static const int size_smallbins = 127;
       static const int size_largebins = 127;
       std::array<Chunk*, size_fastbins> fastbins;
       Chunk* unsortedbin;
       std::array<Chunk*, size_smallbins> smallbins;
       std::array<Chunk*, size_largebins> largebins;
       void* top;
       size_t top_size;
       void printFastbins() const; 
       void printUnsortedbin() const; 
       void printTop() const;
       
       static constexpr void* addr_ub = (void*) 0xdeadbeefdeadbeef;

       Arena() {
           for (int i = 0; i < size_fastbins; i++) {
                fastbins[i] = nullptr; 
           }
           unsortedbin = nullptr;
           for (int i = 0; i < size_largebins; i++) {
                smallbins[i] = nullptr; 
                largebins[i] = nullptr; 
           }
           
           top = nullptr;
           top_size = 0x21001;
       }
};


void Arena::printFastbins() const {
    cout << "=== Fastbins ===" << endl;
    for (int i = 0; i < 7; i++) {
        if (fastbins[i] == nullptr) {
            printf("\t(0x%x) fastbin[%d] = %p\n", 0x20+i*0x10, i, nullptr);
        } else {
            printf("\t(0x%x) fastbin[%d] = %p", 0x20+i*0x10, i, fastbins[i]->get_addr());
            Chunk* tmp = fastbins[i];
            while (tmp->get_next() != nullptr) {
                tmp = tmp->get_next();
                printf(" => %p", tmp->get_addr());
            }
            printf("\n");
        }
    }
}


void Arena::printUnsortedbin() const {
    cout << "=== Unsortedbin ===" << endl;
    Chunk* current = unsortedbin;
    int idx = 0;
    while (true) {
        printf("\t[%02d] %p \n", idx, current->get_addr());
        current = current->get_next();
        idx++;
        if (current == unsortedbin) 
            break;
    } 
}   


void Arena::printTop() const {
    cout << "=== top ===" << endl;
    printf("\ttop = %p (size: 0x%lx)\n", top, (uint64_t)top_size);
}

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


class MemoryHistory {
    std::vector<Step> steps;
    public:
       void loadLog(const std::string fileName);
       void loadDump(const std::string fileName, char* memory);
       void printSteps() const;
       void printStepByStep() const;
       const std::vector<Step> getSteps() const;
};
MemoryHistory memoryHistory;

const std::vector<Step> MemoryHistory::getSteps() const {
    return steps;
}

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

void MemoryHistory::loadDump(const std::string fileName, char* memory) {
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
    ifs_dump.read((char*)memory, size);
}

void MemoryHistory::printSteps() const {
    for (const auto& s : steps) {
        cout << s.toString() << endl;
    }
}

void MemoryHistory::printStepByStep() const {
    for (size_t i = 0; i < steps.size(); i++) {
        size_t j = 0;
        cout << "==================================================" << endl;
        for (const auto& s : steps) {
            if (i < j)
                break;
            j++;
            cout << s.toString() << endl;
        }
    }
}

 
class Mem {
    public:
        static const size_t size = 0x21000;
        unsigned char memory[size];
        // void free(Chunk& ch);
        void free(void* ptr);
        // void malloc(Chunk& ch);
        void malloc(void* ptr, size_t size);
        void analyzeSteps(MemoryHistory* mh);
        void printAllChunks() const;
        void printUsedChunks() const;
        void printFreedChunks() const;
        int find_idx_chunk(void* addr);
    private:
        Arena main_arena;
        std::vector<Chunk> chunks;
        
};

static Mem mem;

int Mem::find_idx_chunk(void* addr) {
    int idx = 0;
    for (auto& c : chunks) {
        if (addr == c.get_ptr())
            return idx;
        idx++;
    }
    return -1;
}


void Mem::malloc(void* ptr, size_t size) {
    cout << "malloc process" << endl;
    
    // fastbinの確認
    if (size <= Chunk::global_max_fast) {
        uint32_t idx_fb = (Chunk::reqeuset2size(size) - Chunk::MIN_CHUNK_SIZE) / 0x10;
        Chunk*& pch = mem.main_arena.fastbins[idx_fb];
        if (pch != nullptr  ) {
            // chunk reuse 
            pch->set_isUsed(true);
            pch = pch->get_next();
            return;
        }
    }

    // smallbin/largebinの確認
    // unsortbinの確認
    if (main_arena.unsortedbin != nullptr) {
        printf("\tub = %p\n", main_arena.unsortedbin->get_addr());
        Chunk*& ub = main_arena.unsortedbin;
        

        
    } 
    
    // topからチャンクを切り出す.
    chunks.emplace_back(ptr, size);
    main_arena.top = (void*)((uint64_t)ptr + size);
    main_arena.top_size -= size;
     
    sort(chunks.begin(), chunks.end());
}
void Mem::free(void* addr) {
    cout << "free process" << endl;
    int idx = find_idx_chunk(addr); 
    if (idx < 0) {
        cout << "unknown address" << endl;
        exit(1);
    }
    Chunk& ch = chunks[idx];
    // fastbin 
    if (ch.get_size() <= Chunk::global_max_fast) {
        cout << "\tfastbin process" << endl;
        uint32_t idx_fb = (ch.get_size() - Chunk::MIN_CHUNK_SIZE) / 0x10;
        if (mem.main_arena.fastbins[idx_fb] ==  nullptr  ) { //  fastbinが空
            mem.main_arena.fastbins[idx_fb] = &ch;
            ch.set_fd(nullptr);
            ch.set_fd_ch(nullptr);
        } else {
            // 既にfastbinにチャンクが存在するのでlinkする
            Chunk* tmp = mem.main_arena.fastbins[idx_fb];
            mem.main_arena.fastbins[idx_fb] = &ch;
            ch.set_fd(tmp->get_addr());
            ch.set_fd_ch(tmp);
        }
        ch.set_isUsed(false);
        cout << "\tinsert fastbin" << endl;
        return;
    }

    
    // smallbin/largebin 
    // prev_inuseの確認(直前がfastbinでなくfreedだったら統合する)
    //
    // 直下がtopだったらtopの位置とサイズを更新する
    if ((void*)((uint64_t)ch.get_addr() + ch.get_size()) == main_arena.top) {
        main_arena.top = ch.get_addr();
        main_arena.top_size += ch.get_size();
        return;
    }

    if (main_arena.unsortedbin == nullptr) {
        main_arena.unsortedbin = &ch;
        ch.set_isUsed(false);
        ch.set_fd((void*)Arena::addr_ub);
        ch.set_bk((void*)Arena::addr_ub);
        ch.set_fd_ch((Chunk*)&ch);
        ch.set_bk_ch((Chunk*)&ch);
    } else {
        ch.set_isUsed(false);
        
        // unsortedbinに繋がるチャンクの最終を探す
        Chunk*& tmp = main_arena.unsortedbin;
        while (true) {
            void* fd = tmp->get_fd();
            if (fd == (void*)Arena::addr_ub) 
                break;
            tmp = tmp->get_next();
        }
        ch.set_fd((void*)Arena::addr_ub);
        ch.set_bk((void*)tmp->get_addr());
        ch.set_fd_ch((Chunk*)tmp);
        ch.set_bk_ch((Chunk*)&ch);
        tmp->set_fd((void*)tmp->get_addr());
        tmp->set_bk((void*)Arena::addr_ub);
        tmp->set_fd_ch((Chunk*)&ch);
        tmp->set_bk_ch((Chunk*)tmp);
        main_arena.unsortedbin = &ch;
        return;
    }

}


void Mem::printAllChunks() const {
    cout << "=== All Chunks ====" << endl;
    for (auto& c : chunks) {
        printf("\t%lx:%lx:%d\n", (uint64_t)c.get_addr(),  (uint64_t)c.get_size(), c.isFree());
    }
}

void Mem::printUsedChunks() const {
    cout << "=== Used Chunks ====" << endl;
    for(auto& c: chunks) {
        if (!c.isFree()) 
            printf("\t%lx:%lx\n", (uint64_t)c.get_addr(),  (uint64_t)c.get_size());
    }
    cout << "====================" << endl;
}

void Mem::printFreedChunks() const {
    cout << "=== Freed Chunks ====" << endl;
    for(auto& c: chunks) {
        if (c.isFree()) 
            printf("\t%lx:%lx\n", (uint64_t)c.get_addr(),  (uint64_t)c.get_size());
    }
    cout << "=====================" << endl;
}


void Mem::analyzeSteps(MemoryHistory* mh) {
    for (auto& s : mh->getSteps()) {
        cout << s.toString() << endl;
        switch (s.get_function()) {
            case MALLOC:
            {
                mem.malloc(s.get_ptr(), s.get_size());
                break;
            }
            case FREE:
            {
                mem.free(s.get_ptr());
                break;
            }
        };

    }

    printUsedChunks();
    printFreedChunks();
    main_arena.printFastbins();
    main_arena.printUnsortedbin();
    main_arena.printTop();
}


int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: filename" << endl;
        exit(1);
    }
    string path = "/tmp/trace_heap/";
    path = path + argv[1];
    
    memoryHistory.loadLog(path);
    memoryHistory.loadDump(path, (char*)mem.memory);
    
    while(1) {
        cout << "================================\n1. print steps\n2. print step by step\n3. analyze steps\n" << endl;
        int choice;
        cin >> choice;
        switch (choice) {
            case 1:
                memoryHistory.printSteps();
                break;
            case 2:
                memoryHistory.printStepByStep();
                break;
            case 3:
                mem.analyzeSteps(&memoryHistory);
                break;
        }
    }
    return 0;
}
