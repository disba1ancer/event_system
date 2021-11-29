#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <map>
#include <type_traits>

namespace evt {

template <auto m>
struct MemFnToFunc;

namespace impl {

template <typename T>
struct MemFnToFuncAddRef {
    using Type = T&;
};

template <typename T>
struct MemFnToFuncAddRef<T&> {
    using Type = T&;
};

template <typename T>
struct MemFnToFuncAddRef<T&&> {
    using Type = T&&;
};

template <typename T>
using MemFnToFuncAddRefT = typename MemFnToFuncAddRef<T>::Type;

template <typename Sig, typename Cal>
struct ExtractCallable;

template <typename Ret, typename ... Args, typename Cal>
struct ExtractCallable<Ret(Args...), Cal> {
    static Ret Function(void* callable, Args ... args) {
        return static_cast<MemFnToFuncAddRefT<Cal>>(*(std::remove_reference_t<Cal>*)(callable))(args...);
    }
};

template <typename Sig, auto m>
struct ExtractMember;

#define GENERATE(cvr)\
template <typename Ret, typename Obj, typename ... Args, Ret(Obj::*m)(Args...) cvr>\
struct ExtractMember<Ret(Args...), m> {\
    static Ret Function(void* obj, Args ... args)\
    {\
        return (static_cast<MemFnToFuncAddRefT<Obj cvr>>(*static_cast<Obj*>(obj)).*m)(args...);\
    }\
};

GENERATE()
GENERATE(const)
GENERATE(volatile)
GENERATE(const volatile)
GENERATE(&)
GENERATE(const&)
GENERATE(volatile&)
GENERATE(const volatile&)
GENERATE(&&)
GENERATE(const&&)
GENERATE(volatile&&)
GENERATE(const volatile&&)
#undef GENERATE

template <typename T>
class SubrangeFromPair {
    T beginIter;
    T endIter;
public:
    SubrangeFromPair(const std::pair<T, T>& pair) : beginIter(pair.first), endIter(pair.second)
    {}
    friend T begin(const SubrangeFromPair& sbr)
    {
        return sbr.beginIter;
    }
    friend T end(const SubrangeFromPair& sbr)
    {
        return sbr.endIter;
    }
};

template <typename T>
struct AddVoidArg;

template <typename Ret, typename ... Args>
struct AddVoidArg<Ret(Args...)> {
    using Type = Ret(void*, Args...);
};

template <typename T>
using AddVoidArgT = typename AddVoidArg<T>::Type;

template <typename T>
struct IsMemFnToFunc : std::false_type {};

template <template <auto> typename T, auto m>
struct IsMemFnToFunc<T<m>> : std::true_type {};

template <typename T>
constexpr auto& IsMemFnToFuncV = IsMemFnToFunc<T>::value;

}

#define GENERATE(cvr)\
template <typename Ret, typename Obj, typename ... Args, Ret (Obj::*m)(Args...) cvr>\
struct MemFnToFunc<m> {\
    using ObjType = Obj cvr;\
    ObjType& self;\
    MemFnToFunc(typename impl::MemFnToFuncAddRef<ObjType>::Type obj) : self(obj) {}\
    Ret operator ()(Args ... args) const {\
        return (static_cast<ObjType>(self).*m)(args...);\
    }\
};

GENERATE()
GENERATE(const)
GENERATE(volatile)
GENERATE(const volatile)
GENERATE(&)
GENERATE(const&)
GENERATE(volatile&)
GENERATE(const volatile&)
GENERATE(&&)
GENERATE(const&&)
GENERATE(volatile&&)
GENERATE(const volatile&&)
#undef GENERATE

template <typename T>
struct EventTraits {
    using HandlerType = typename T::HandlerType;
};

class EventManager {
    using EventId = void*;
    struct Key {
        EventId id;
        void* sender;
    };
    struct KeyCompare {
        bool operator()(const Key& a, const Key& b) const
        {
            std::less<void*> less;
            std::equal_to<void*> equal;
            return less(a.id, b.id) || (equal(a.id, b.id) && less(a.sender, b.sender));
        }
    };

    struct Handler {
        void(*method)();
        union {
            void* obj;
            void(*static_func)();
        };
    };
    std::multimap<Key, Handler, KeyCompare> handlers;

public:
    template <typename Evt, typename Snd>
    void BindEventHandler(Evt& eventId, Snd& eventSender, typename EventTraits<Evt>::HandlerType& callable)
    {
        Handler handler;
        handler.method = nullptr;
        handler.static_func = reinterpret_cast<void(*)()>(&callable);
        handlers.insert(std::make_pair(Key{(void*)&eventId, (void*)&eventSender}, handler));
    }
    template <typename Evt, typename Snd, auto m, typename = void /* SFINAE required */>
    void BindEventHandler(Evt& eventId, Snd& eventSender, const MemFnToFunc<m>& callable)
    {
        Handler handler;
        using HandlerType = typename EventTraits<std::remove_cv_t<Evt>>::HandlerType;
        handler.method = reinterpret_cast<void(*)()>(impl::ExtractMember<HandlerType, m>::Function);
        handler.obj = (void*)(&callable.self);
        handlers.insert(std::make_pair(Key{(void*)&eventId, (void*)&eventSender}, handler));
    }
    template <typename Evt, typename Snd, typename C, typename = std::enable_if_t<!std::is_function_v<C> && !impl::IsMemFnToFuncV<C>>>
    void BindEventHandler(Evt& eventId, Snd& eventSender, C& callable)
    {
        Handler handler;
        using HandlerType = typename EventTraits<std::remove_cv_t<Evt>>::HandlerType;
        handler.method = reinterpret_cast<void(*)()>(&impl::ExtractCallable<HandlerType, C>::Function);
        handler.obj = (void*)&callable;
        handlers.insert(std::make_pair(Key{(void*)&eventId, (void*)&eventSender}, handler));
    }
    template <typename Evt, typename Snd, typename ... Args>
    void SendEvent(Evt& eventId, Snd& eventSender, Args&& ... args)
    {
        for (auto& [key, value] : impl::SubrangeFromPair(handlers.equal_range(Key{(void*)&eventId, (void*)&eventSender}))) {
            using HandlerType = typename EventTraits<std::remove_cv_t<Evt>>::HandlerType;
            if (value.method == nullptr) {
                (reinterpret_cast<HandlerType*>(value.static_func))(args...);
            } else {
                (reinterpret_cast<impl::AddVoidArgT<HandlerType>*>(value.method))(value.obj, args...);
            }
        }
    }
};

}

#endif // EVENT_MANAGER_H
