#include <vector>
#include <variant>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#define PAGESIZE 0x1000

inline void error(const char *str) {
  perror(str);
  exit(-1);
}

class structinfo_t {
public:
  std::vector<std::vector<uint64_t>> structinfo;

  auto get_size() -> uint64_t {
    return structinfo.size();
  }

  auto get_deep(uint64_t n) -> uint64_t {
    return structinfo.at(n).size()-1;
  }

  auto get(uint64_t a, uint64_t b) -> uint64_t {
    return structinfo.at(a).at(b);
  }

  auto push(std::vector<uint64_t> vec) -> void {
    structinfo.push_back(vec);
  }

  auto push_end(uint64_t val) -> void {
    structinfo.at(get_size()-1).push_back(val);
  }

  auto incr_end(uint64_t pos) -> void {
    structinfo.at(get_size()-1).at(pos)++;
  }

  auto get_last(uint64_t n) -> uint64_t {
    return structinfo.at(n).at(structinfo.at(n).size()-1);
  }

  auto get_vec(uint64_t pos) -> std::vector<uint64_t> {
    return structinfo.at(pos);
  }
};

class syscall_t {
public:
  syscall_t() : sysno{0}, nargs{0} {};

  uint16_t sysno;
  uint16_t nargs;
  std::vector<uint32_t> nargno;
  std::vector<uint64_t> value;
  structinfo_t sinfo;
};

class sysdevproc_op_t {
public:
  sysdevproc_op_t() : fd{0}, nsize{0}, option{0}, request{0} {};
  ~sysdevproc_op_t() {
    close(fd);
  }

  int32_t fd;
  uint16_t nsize;
  uint8_t option;
  uint64_t request;
  std::vector<uint64_t> value;
  structinfo_t sinfo;
};

class socket_op_t {
public:
  socket_op_t() : fd{0}, nsize{0}, option{0}, request{0}, optname{0} {};
  ~socket_op_t() {
    close(fd);
  }

  int32_t fd;
  uint16_t nsize;
  uint8_t option;
  uint64_t request;
  std::vector<uint64_t> value;
  structinfo_t sinfo;
  int32_t optname;
};

class prog_t {
public:
  prog_t() : nops{0} {};
  ~prog_t() {
    switch(inuse) {
      case 0:
      for(unsigned long i{0}; i < op.sysc->size(); i++) {
        delete op.sysc->at(i);
      }
      delete op.sysc;
      break;
      case 1:
      for(unsigned long i{0}; i < op.sdp->size(); i++) {
        delete op.sdp->at(i);
      }
      delete op.sdp;
      break;
      case 2:
      for(unsigned long i{0}; i < op.sock->size(); i++) {
        delete op.sock->at(i);
      }
      delete op.sock;
      break;
    }
  }

  uint8_t nops;
  uint8_t inuse;
  union {
    std::vector<syscall_t*> *sysc;
    std::vector<sysdevproc_op_t*> *sdp;
    std::vector<socket_op_t*> *sock;
  } op;

  std::string log;

  std::string devname;
  int32_t prot;

  int32_t domain;
  int32_t type;
};

class fuzzinfo_t {
  std::vector<prog_t*> corpus;
public:

  virtual void add_corpus(prog_t *p) {
    corpus.push_back(p);
  }

  virtual prog_t *get_corpus() {
    prog_t *tmp;

    if(!corpus.size()) {
      tmp = nullptr;
    } else {
      tmp = corpus.back();
      corpus.pop_back();
    }
    return tmp;
  }

  virtual uint64_t get_corpus_count() {
    return corpus.size();
  }
};

#define SETVAL(x, y) {\
  deref(x, &offsets)[perstruct_cnt.back()] = y.at(i);\
  perstruct_cnt.back()++;\
}

#define REALLOC_STRUCT(x) {\
  {\
    size_t tmp{offsets.back()};\
    offsets.pop_back();\
    size.at(size.size()-1) += 8;\
    auto ptr{reinterpret_cast<void*>(deref(x, &offsets)[perstruct_cnt.at(perstruct_cnt.size()-2)-1])};\
    size_t i{0};\
    for(; i < ptrs.size(); i++) {\
      if(ptrs.at(i) == ptr) {\
        ptrs.at(i) = realloc(reinterpret_cast<void*>(ptr),size.at(size.size()-1));\
        break;\
      }\
    }\
    deref(x, &offsets)[perstruct_cnt.at(perstruct_cnt.size()-2)-1] = reinterpret_cast<uint64_t>(ptrs.at(i));\
    offsets.push_back(tmp);\
  }\
}

#define ALLOC_STRUCT(x) {\
  {\
    bool existing{false};\
    auto buf{malloc(8)};\
    deref(x, &offsets)[perstruct_cnt.back()] = reinterpret_cast<uint64_t>(buf);\
    for(auto e : ptrs) if(e == buf) existing = true;\
    if(!existing) ptrs.push_back(buf);\
    offsets.push_back(perstruct_cnt.back());\
    perstruct_cnt.back()++;\
    perstruct_cnt.push_back(0);\
    size.push_back(8);\
  }\
}

template <typename T>
inline auto check_smaller_before(uint64_t start, uint64_t c, T* s) -> bool {
  for(int64_t i{static_cast<int64_t>(start)+1}; i >= 0; i--) {
    if(s->sinfo.get_deep(i) >= c) break;
    if(s->sinfo.get_deep(i) < c) return true;
  }

  return false;
}

inline auto deref(uint64_t *in, std::vector<size_t>* offsets) -> uint64_t* {
  uint64_t *tmp{in};

  for(uint64_t i{0}; i < offsets->size(); i++) {
    tmp = reinterpret_cast<uint64_t*>(tmp[offsets->at(i)]);
  }

  return tmp;
}

extern std::vector<std::string> virtual_dev_names;

auto get_random(uint64_t, uint64_t) -> uint64_t;
auto flog_program(prog_t *, int32_t) -> void;
auto execute_program(prog_t*) -> pid_t;

auto mutate_prog(prog_t *p) -> void;

template <typename... T>
auto exec_syscall(uint16_t, T...) -> void;
auto exec_syscall(uint16_t) -> void;
auto execute(syscall_t*) -> void;
auto create_syscall() -> syscall_t*;
auto create_program1() -> prog_t*;

auto open_device(prog_t*) -> int32_t;
auto execute(sysdevproc_op_t*) -> void;
auto create_sysdevprocop() -> sysdevproc_op_t*;
auto create_program2() -> prog_t*;

auto open_socket(prog_t*) -> int32_t;
auto execute(socket_op_t*) -> void;
auto create_socketop() -> socket_op_t*;
auto create_program3() -> prog_t*;
