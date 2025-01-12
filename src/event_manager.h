#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <set>
#include <type_traits>
#include <concepts>

namespace evt {

template <auto m>
struct to_static_fn;

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
        return (const_cast<MemFnToFuncAddRefT<Obj cvr>>(*static_cast<const volatile Obj*>(obj)).*m)(args...);\
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

template <typename Sig>
struct wrap_static;

template <typename Ret, typename...Args>
struct wrap_static<Ret(Args...)> {
    static Ret function(void* obj, Args ... args)
    {
        return reinterpret_cast<Ret(*)(Args...)>(obj)(args...);
    }
};

template <typename T>
class to_range {
    T beginIter;
    T endIter;
public:
    to_range(const std::pair<T, T>& pair) : beginIter(pair.first), endIter(pair.second)
    {}
    friend T begin(const to_range& sbr)
    {
        return sbr.beginIter;
    }
    friend T end(const to_range& sbr)
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
struct to_static_fn<m> {\
    using ObjType = Obj cvr;\
    ObjType& self;\
    to_static_fn(typename impl::MemFnToFuncAddRef<ObjType>::Type obj) : self(obj) {}\
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

template <typename Ret, typename Obj>
struct member_func_ptr;

#define GENERATE(cvr)\
template <typename Ret, typename Obj, typename ... Args>\
struct member_func_ptr<Ret(Args...), Obj cvr> {\
    using type = Ret(Obj::*)(Args...) cvr;\
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

template <typename Ret, typename Obj>
using member_func_ptr_t = member_func_ptr<Ret, Obj>::type;

template <auto m>
struct fn_tag_t {};

template <auto m>
constexpr auto fn_tag = fn_tag_t<m>{};

template <auto en>
requires std::is_scoped_enum_v<decltype(en)>
struct event_traits;

template <class EventEnum>
requires std::is_scoped_enum_v<EventEnum>
class EventManager {
    using EventId = EventEnum;
    struct Key {
        std::underlying_type_t<EventId> id;
        void* observer;
        void(*callback)();
    };
    struct KeyCompare {
        bool operator()(const Key& a, const Key& b) const
        {
            auto ord = a.id <=> b.id;
            if (ord != 0) {
                return ord < 0;
            }
            auto pA = reinterpret_cast<std::uintptr_t>(a.observer);
            auto pB = reinterpret_cast<std::uintptr_t>(b.observer);
            if ((ord = pA <=> pB) != 0) {
                return ord < 0;
            }
            pA = reinterpret_cast<std::uintptr_t>(reinterpret_cast<void*>(a.callback));
            pB = reinterpret_cast<std::uintptr_t>(reinterpret_cast<void*>(b.callback));
            return pA <=> pB < 0;
        }
    };

    bool RegisterInt(EventId event, void* object, void(*callback)())
    {
        auto [it, r] = handlers.emplace(std::to_underlying(event), object, callback);
        return r;
    }

    void UnregisterInt(EventId event, void* object, void(*callback)())
    {
        handlers.erase(Key{std::to_underlying(event), object, callback});
    }
    template <EventId event>
    using handler = typename event_traits<event>::handler;
public:
    template <EventId event>
    bool Register(void* object, impl::AddVoidArgT<handler<event>>* callback)
    {
        return RegisterInt(event, object, reinterpret_cast<void(*)()>(callback));
    }
    template <EventId event>
    void Unregister(void* object, impl::AddVoidArgT<handler<event>>* callback)
    {
        UnregisterInt(event, object, reinterpret_cast<void(*)()>(callback));
    }
    template <EventId event>
    bool Register(handler<event>* m)
    {
        auto object = reinterpret_cast<void*>(m);
        auto callback = impl::wrap_static<handler<event>>::function;
        return Register<event>(object, callback);
    }
    template <EventId event>
    void Unregister(handler<event>* m)
    {
        auto object = reinterpret_cast<void*>(m);
        auto callback = impl::wrap_static<handler<event>>::function;
        Unregister<event>(object, callback);
    }
    template <EventId event, typename Obs, auto m>
    requires (
        std::same_as<decltype(m), member_func_ptr_t<handler<event>, Obs&&>> ||
        std::same_as<decltype(m), member_func_ptr_t<handler<event>, std::remove_reference_t<Obs>>>
    )
    bool Register(Obs&& obs, fn_tag_t<m>)
    {
        using handler_t = handler<event>;
        auto callback = impl::ExtractMember<handler_t, m>::Function;
        auto object = static_cast<void*>(std::addressof(obs));
        return Register<event>(object, callback);
    }
    template <EventId event, typename Obs, auto m>
    requires (
        std::same_as<decltype(m), member_func_ptr_t<handler<event>, Obs&&>> ||
        std::same_as<decltype(m), member_func_ptr_t<handler<event>, std::remove_reference_t<Obs>>>
    )
    void Unregister(Obs&& obs, fn_tag_t<m>)
    {
        using handler_t = handler<event>;
        auto callback = impl::ExtractMember<handler_t, m>::Function;
        auto object = static_cast<void*>(std::addressof(obs));
        Unregister<event>(object, callback);
    }
    template <EventId event, typename C>
    requires (!std::is_function_v<C> && !impl::IsMemFnToFuncV<C>)
    bool Register(C&& callable)
    {
        using handler_t = handler<event>;
        auto callback = impl::ExtractCallable<handler_t, C>::Function;
        auto object = static_cast<void*>(std::addressof(callable));
        return Register<event>(object, callback);
    }
    template <EventId event, typename C>
    requires (!std::is_function_v<C> && !impl::IsMemFnToFuncV<C>)
    void Unregister(C&& callable)
    {
        using handler_t = handler<event>;
        auto callback = impl::ExtractCallable<handler_t, C>::Function;
        auto object = static_cast<void*>(std::addressof(callable));
        Unregister<event>(object, callback);
    }
    template <EventId event, typename ... Args>
    void Send(Args&& ... args)
    {
        constexpr auto eid = std::to_underlying(event);
        auto zeroptr = reinterpret_cast<void*>(0);
        auto zerofptr = reinterpret_cast<void(*)()>(zeroptr);
        Key bKey{eid, zeroptr, zerofptr};
        Key eKey{eid + 1, zeroptr, zerofptr};
        auto begin = handlers.lower_bound(bKey);
        auto end = handlers.lower_bound(eKey);
        for (auto& hndlr : impl::to_range(std::pair{begin, end})) {
            using handler_t = handler<event>;
            auto observer = hndlr.observer;
            auto callback = reinterpret_cast<impl::AddVoidArgT<handler_t>*>(hndlr.callback);
            callback(observer, std::forward<Args>(args)...);
        }
    }
private:
    std::set<Key, KeyCompare> handlers;
};

}

#endif // EVENT_MANAGER_H
