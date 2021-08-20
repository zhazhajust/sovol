#ifndef _SOVOL_LUA_HH
#define _SOVOL_LUA_HH 1

#include <lua.hpp>
#include <memory>
#include <sstream>
#include <string>

class Lua {
 private:
  lua_State* L;
  void push(const char* s) { lua_pushstring(L, s); };
  void push(const std::string& s) { lua_pushstring(L, s.c_str()); };
  template <typename Integer,
            std::enable_if_t<std::is_integral_v<Integer>, bool> = true>
  void push(Integer v) {
    lua_pushinteger(L, v);
  };
  template <typename Floating,
            std::enable_if_t<std::is_floating_point_v<Floating>, bool> = true>
  void push(Floating v) {
    lua_pushnumber(L, v);
  };
  template <typename Integer,
            std::enable_if_t<std::is_integral_v<Integer>, bool> = true>
  Integer as(int index = -1) {
    int isnum;
    Integer r = lua_tointegerx(L, index, &isnum);
    if (!isnum) {
      throw std::runtime_error("Lua error: Can't convert to integral.");
    }
    return r;
  };
  template <typename Floating,
            std::enable_if_t<std::is_floating_point_v<Floating>, bool> = true>
  Floating as(int index = -1) {
    int isnum;
    Floating r = lua_tonumberx(L, index, &isnum);
    if (!isnum) {
      throw std::runtime_error("Lua error: Can't convert to number.");
    }
    return r;
  }
  template <typename String,
            std::enable_if_t<std::is_same_v<String, std::string>, bool> = true>
  std::string as(int index = -1) {
    const char* r = lua_tostring(L, index);
    if (!r) {
      throw std::runtime_error("Lua error: Can't convert to string.");
    }
    return std::string(r);
  }

 public:
  Lua(const Lua&) = delete;
  Lua& operator=(const Lua&) = delete;
  Lua(const std::string& _path) {
    L = luaL_newstate();
    if (L == nullptr) {
      throw std::runtime_error("Lua error: Failed to new state.");
    }
    luaL_openlibs(L);
    int ret = luaL_loadfile(L, _path.c_str());
    if (ret) {
      std::stringstream ss;
      switch (ret) {
        case LUA_ERRMEM:
          throw std::runtime_error("Lua memory allocation error.");
        case LUA_ERRSYNTAX:
          ss << "Lua syntax error: ";
          break;
        case LUA_ERRFILE:
          ss << "Load lua file failed: ";
          break;
      }
      ss << as<std::string>();
      throw std::runtime_error(ss.str());
    }
    call(LUA_MULTRET);
  };
  ~Lua() {
    if (L != nullptr) {
      lua_close(L);
      L = nullptr;
    }
  };
  bool isNil(int index = -1) { return lua_isnil(L, index); }
  template <typename... Args>
  void call(int nresults, Args&&... args) {
    if (!lua_isfunction(L, -1)) {
      throw std::runtime_error("Input lua script error: Not a function");
    }
    std::tuple<Args...> tuple(std::forward<Args>(args)...);
    std::apply([this](Args... args) { (push(args), ...); }, tuple);
    int ret = lua_pcall(L, sizeof...(Args), nresults, 0);
    if (ret) {
      std::stringstream ss;
      switch (ret) {
        case LUA_ERRMEM:
          throw std::runtime_error("Lua memory allocation error.");
        case LUA_ERRRUN:
          ss << "Lua runtime error: ";
          break;
        case LUA_ERRERR:
          ss << "Lua error while running the message handler: ";
          break;
      }
      ss << as<std::string>();
      throw std::runtime_error(ss.str());
    }
  }
  template <typename U, typename T>
  U getField(T key, U defaultValue) {
    int type = visitField(key);
    U r;
    if (type == LUA_TNIL) {
      r = defaultValue;
    } else {
      r = as<U>();
    }
    lua_pop(L, 1);
    return r;
  };
  template <typename U, typename T>
  U getField(T key) {
    int type = visitField(key);
    U r = as<U>();
    lua_pop(L, 1);
    return r;
  };
  template <typename U>
  U getGlobal(const char* name, U defaultValue) {
    int type = visitGlobal(name);
    U r;
    if (type == LUA_TNIL) {
      r = defaultValue;
    } else {
      r = as<U>();
    }
    lua_pop(L, 1);
    return r;
  };
  template <typename U>
  U getGlobal(const char* name) {
    int type = visitGlobal(name);
    U r = as<U>();
    lua_pop(L, 1);
    return r;
  };
  template <typename T>
  int visitField(T key) {
    if (!lua_istable(L, -1)) {
      throw std::runtime_error("Input lua script error: Not a table");
    }
    push(key);
    return lua_gettable(L, -2);
  }
  int visitGlobal(const char* name) { return lua_getglobal(L, name); }
  void leaveTable() {
    if (!lua_istable(L, -1)) {
      throw std::runtime_error("Input lua script error: Not a table");
    }
    lua_pop(L, 1);
  }
  static std::string path;
  static Lua& getInstance() {
    thread_local std::unique_ptr<Lua> instance = std::make_unique<Lua>(path);
    return *instance;
  }
};

std::string Lua::path;

#endif
