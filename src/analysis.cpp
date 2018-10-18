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
        char* addr;
        size_t prev_size;
        size_t size;
        char* fd;
        char* bk;
        char* fd_nextsize;
        char* bk_nextsize;
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
        Chunk(char* _addr, size_t _size)
            : isUsed(true)
            , addr(_addr - 0x10)
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
        char* get_addr() const { return addr; }
        char* get_ptr() const { return addr + 0x10; }
        size_t get_size() const { return size; }
        bool isFree() const { return !isUsed; }
        char* get_fd() const { return fd;}
        char* get_bk() const { return bk;}
        void set_size(size_t s) { size = s; }
        void set_fd(char* ptr) { fd = ptr; }
        void set_bk(char* ptr) { bk = ptr; }
        void set_fd_ch(Chunk* ch) { fd_ch = ch; }
        void set_bk_ch(Chunk* ch) { bk_ch = ch; }
        void set_isUsed(bool _isUsed) { isUsed = _isUsed; }
        Chunk* get_next() const { return fd_ch; }
        Chunk* get_prev() const { return bk_ch; }
        bool operator<(const Chunk& rhs) const {return addr < rhs.addr;}
        void splitChunk(size_t new_size);
        // void setLink()
};


// 仮想的なmain arena
class Arena {
    
    public:
       static const int size_fastbins = 7;
       static const int size_smallbins = 127;
       static const int size_largebins = 127;
       std::array<Chunk*, size_fastbins> fastbins;
       Chunk* unsortedbin;
       std::array<Chunk*, size_smallbins> smallbins{};
       std::array<Chunk*, size_largebins> largebins{};
       char* top; // uint8_t or char
       size_t top_size;
       void printFastbins() const; 
       void printUnsortedbin() const; 
       void printSmallbins() const; 
       void printTop() const;
       
       void insertSmallbins(Chunk* chunk);
       void insertLargebins(Chunk* chunk); 
       void consolidateChunks(Chunk* prev, Chunk* chunk);       

       void unsorted2bins(char* excluded);
       static constexpr char* addr_ub = (char*)0xdeadbeefdeadbeef;
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


void Arena::consolidateChunks(Chunk* prev, Chunk* chunk) {
    size_t new_size = prev->get_size() + chunk->get_size();
    printf("prev %p:%zd\n", prev->get_addr(), prev->get_size());
    printf("next %p:%zd\n", chunk->get_addr(), chunk->get_size());
    prev->set_size(new_size);
}

void Arena::insertSmallbins(Chunk* chunk) {
    int idx = (chunk->get_size() - 0x20) / 0x10;
    if (smallbins[idx] != nullptr) {
        Chunk *tmp = smallbins[idx];
        while (true) {
            if (tmp->get_fd() == Arena::addr_ub)
                break;
            tmp = tmp->get_next();
        }
        tmp->set_fd(chunk->get_addr());
        tmp->set_fd_ch(chunk);
        chunk->set_bk(tmp->get_addr());
        chunk->set_bk_ch(tmp);
    } else { 
        smallbins[idx] = chunk;
        chunk->set_fd(Arena::addr_ub);
        chunk->set_bk(Arena::addr_ub);
        chunk->set_fd_ch(chunk);
        chunk->set_bk_ch(chunk);
    }

}

void Arena::insertLargebins(Chunk* chunk) {
    int idx = (chunk->get_size() - 0x20) / 0x10;
    if (largebins[idx] != nullptr) {
        Chunk *tmp = largebins[idx];
        while (true) {
            if (tmp->get_fd() == Arena::addr_ub)
                break;
            tmp = tmp->get_next();
        }
        tmp->set_fd(chunk->get_addr());
        tmp->set_fd_ch(chunk);
        chunk->set_bk(tmp->get_addr());
        chunk->set_bk_ch(tmp);
    } else { 
        largebins[idx] = chunk;
        chunk->set_fd(Arena::addr_ub);
        chunk->set_bk(Arena::addr_ub);
        chunk->set_fd_ch(chunk);
        chunk->set_bk_ch(chunk);
    }
}

void Arena::unsorted2bins(char* excluded) {
    Chunk* chunk = unsortedbin;
    while (true) {
        if (chunk->get_addr() == excluded)
            chunk = chunk->get_next();
        // size_t size = chunk->get_size();
        chunk = chunk->get_next();
    }
    
}

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
        printf("\t[%02d] %p fd: %p bk: %p\n", idx, current->get_addr(), current->get_fd(), current->get_bk());
        current = current->get_next();
        idx++;
        if (current == unsortedbin) 
            break;
    } 
}   

void Arena::printSmallbins() const {
    cout << "=== Smallbins ===" << endl;
    for (int i=0; i<size_smallbins; i++) {
        if (smallbins[i] != nullptr) {
            printf("\t[%02d] %p fd: %p bk: %p\n", i, smallbins[i]->get_addr(), smallbins[i]->get_fd(), smallbins[i]->get_bk());
        }
    }
}



void Arena::printTop() const {
    cout << "=== top ===" << endl;
    printf("\ttop = %p (size: 0x%zx)\n", top, (uintptr_t)top_size);
}

// heapに対する操作を記憶するクラス
class Step {
    private:
        int function;
        bool isUsed;
        char* ptr;
        size_t size;
        size_t nmemb;
    public:
        Step(int _function, char* _ptr, size_t _size, size_t _nmemb)
            : function(_function)
            , ptr(_ptr)
            , size(_size)
            , nmemb(_nmemb)
        {
        }
        char* get_ptr() const { return ptr; }
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
        char* ptr;
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
        void free(char* ptr);
        void malloc(char* ptr, size_t size);
        void analyzeSteps(MemoryHistory* mh);
        void printAllChunks() const;
        void printUsedChunks() const;
        void printFreedChunks() const;
        int find_idx_chunk(void* addr);
        std::vector<Chunk> chunks;
        Arena main_arena;
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


void Mem::malloc(char* ptr, size_t size) {
    // fastbinの確認
    if (size <= Chunk::global_max_fast) {
        uint32_t idx_fb = (Chunk::reqeuset2size(size) - Chunk::MIN_CHUNK_SIZE) / 0x10;
        Chunk*& pch = mem.main_arena.fastbins[idx_fb];
        if (pch != nullptr  ) {
            // chunk reuse 
            pch->set_isUsed(true);
            pch = pch->get_next();
            sort(chunks.begin(), chunks.end());
            return;
        }
    }

    // smallbin/largebinの確認
    // unsortbinの確認
    if (main_arena.unsortedbin != nullptr) {
        printf("\tub = %p\n", main_arena.unsortedbin->get_addr());
        Chunk* ub;
        Chunk* tmp = main_arena.unsortedbin;
        while (true) {
            printf("\ttmp = %p\n", tmp->get_addr());
            char* fd = tmp->get_fd();
            if (fd == (void*)Arena::addr_ub) 
                break;
            tmp = tmp->get_next();
        }
        ub = tmp;
        printf("\ttarget ub = %p\n", ub->get_addr());
        
        // chunkの分割処理
        // サイズが同じなら，そのまま
        if (ub->get_size() == Chunk::reqeuset2size(size)) {
            cout << "just size found!" << endl;
            ub->set_isUsed(true);
            // unlink処理 
            Chunk* prev = ub->get_prev();
            Chunk* next = ub->get_next();
            prev->set_fd(next->get_fd());
            next->set_bk(prev->get_bk());
        } else {
            main_arena.printUnsortedbin();
            tmp = main_arena.unsortedbin;
            while (true) {
            printf("\ttmp = %p\n", tmp->get_addr());
               char* fd = tmp->get_fd();
                if (fd == (void*)Arena::addr_ub) 
                    break;
                if (tmp != ub) {
                    if (tmp->get_size() < 0x400) {
                        // smallbin
                        main_arena.insertSmallbins(tmp);
                    } else {
                        // largebin
                        main_arena.insertLargebins(tmp);
                    }
                }
                tmp = tmp->get_next();
            }
            if (Chunk::reqeuset2size(size) > ub->get_size()) {
                cout << "good chunks doesn't exist" << endl;
                // topからチャンクを切り出す.
                chunks.emplace_back(ptr, size);
                main_arena.top = ptr + size;
                main_arena.top_size -= size;

            } else {
                ub->set_isUsed(true); 
                ub->splitChunk(ub->get_size() - Chunk::reqeuset2size(size));
                printf("%p:%zd\n", ub->get_addr(), ub->get_size()); 
                printf("%p\n", ub);
                main_arena.printUnsortedbin();
            }
        }

        sort(chunks.begin(), chunks.end());
        return ; 
    } 
    
    // topからチャンクを切り出す.
    chunks.emplace_back(ptr, size);
    main_arena.top = ptr + size;
    main_arena.top_size -= size;
     
    sort(chunks.begin(), chunks.end());
}
void Mem::free(char* addr) {
    int idx = find_idx_chunk(addr); 
    if (idx < 0) {
        cout << "unknown address" << endl;
        exit(1);
    }
    Chunk& ch = chunks[idx];
    // fastbin 
    if (ch.get_size() <= Chunk::global_max_fast) {
        uint32_t idx_fb = (ch.get_size() - Chunk::MIN_CHUNK_SIZE) / 0x10; // size_t
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
    Chunk& prev = chunks[idx - 1];
    if (prev.isFree() == true && prev.get_size() > 0x80) {
        cout << "consolidate chunks" << endl;
        main_arena.consolidateChunks(&prev, &ch);
        chunks.erase(chunks.begin() + idx);
    }


    // 直下がtopだったらtopの位置とサイズを更新する
//:    if (main_arena.top - ch.get_addr() == ch.get_size()) {
    if ((void*)((uint64_t)ch.get_addr() + ch.get_size()) == main_arena.top) {
        main_arena.top = ch.get_addr();
        main_arena.top_size += ch.get_size();
        return;
    }

    if (main_arena.unsortedbin == nullptr) {
        main_arena.unsortedbin = &ch;
        ch.set_isUsed(false);
        ch.set_fd(Arena::addr_ub);
        ch.set_bk(Arena::addr_ub);
        ch.set_fd_ch((Chunk*)&ch);
        ch.set_bk_ch((Chunk*)&ch);
    } else {
        ch.set_isUsed(false);
        // unsortedbinに繋がるチャンクの最終を探す
        Chunk*& tmp = main_arena.unsortedbin;
        printf("ch = %p\n", ch.get_addr());
        while (true) {
            printf("\ttmp = %p\n", tmp->get_addr());
            char* fd = tmp->get_fd();
            if (fd == Arena::addr_ub) 
                break;
            tmp = tmp->get_next();
        }
        // 関数にして
        cout << tmp->get_addr() << endl;
        ch.set_fd(tmp->get_addr());
        ch.set_bk(Arena::addr_ub);
        ch.set_fd_ch((Chunk*)tmp);
        ch.set_bk_ch((Chunk*)&ch);
        tmp->set_fd(Arena::addr_ub);
        tmp->set_bk(ch.get_addr());
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
    main_arena.printSmallbins();
}


void Chunk::splitChunk(size_t new_size) {
    size_t remain = size - new_size;
    char* new_addr = addr + remain;
    printf("splitChunk: new_addr:0x%p new_size = %zd\n", new_addr, new_size);
    size = remain;
    
    Chunk* new_chunk = new Chunk(new_addr+0x10, new_size-0x10);
    new_chunk->set_fd(Arena::addr_ub);
    new_chunk->set_bk(Arena::addr_ub);
    new_chunk->set_fd_ch(new_chunk);
    new_chunk->set_bk_ch(new_chunk);
    mem.main_arena.unsortedbin = new_chunk;
    new_chunk->set_isUsed(false);
    mem.chunks.push_back(*new_chunk);
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
