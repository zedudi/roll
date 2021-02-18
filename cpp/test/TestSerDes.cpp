#include "RpcSerdes.h"
#include "RpcStlMap.h"
#include "RpcStlSet.h"
#include "RpcStlList.h"
#include "RpcStlTuple.h"
#include "RpcStlArray.h"

#include "1test/Test.h"

#include <memory>
#include <string.h>

TEST_GROUP(SerDes) 
{
    using a = std::initializer_list<int>;

    class MockStream
    {
        std::unique_ptr<char[]> buffer;
        char *end;

        class Accessor
        {
            friend MockStream;
            char *ptr, *const end;
            inline Accessor(char* ptr, char* end): ptr(ptr), end(end) {}

        public:
            template<class T>
            bool write(const T& v)
            {
                static constexpr auto size = sizeof(T);
                if(size <= end - ptr)
                {
                    memcpy(ptr, &v, size);
                    ptr += size;
                    return true;
                }

                return false;
            }

            template<class T>
            bool read(T& v)
            {
                static constexpr auto size = sizeof(T);
                if(size <= end - ptr)
                {
                    memcpy(&v, ptr, size);
                    ptr += size;
                    return true;
                }

                return false;
            }

            template<class T>
            bool skip(size_t size)
            {
                if(size <= end - ptr)
                {
                    ptr += size;
                    return true;
                }

                return false;
            }
        };

    public:
        inline auto access() {
            return Accessor(buffer.get(), end);
        }

        inline bool truncateAt(size_t offset)
        {
            auto start = buffer.get();

            if(offset < end - start)
            {
                end = start + offset;
                return true;
            }

            return false;
        }

        inline MockStream(size_t size): buffer(new char[size]), end(buffer.get() + size) {};
    };

    template<class... C>
    auto write(C&&... c)
    {
        MockStream stream(determineSize<C...>(std::forward<C>(c)...));
        auto a = stream.access();
        CHECK(serialize<C...>(a, std::forward<C>(c)...));
        CHECK(!a.write('\0'));
        return stream;
    }

    template<class... C>
    bool read(MockStream &stream, C&&... c)
    {
        bool done = false;
        auto b = stream.access();
        bool deserOk = deserialize<C...>(b, [&done, exp{std::make_tuple<C...>(std::forward<C>(c)...)}](auto&&... args){
            CHECK(std::make_tuple(args...) == exp);
            done = true;
        });

        return deserOk && done;
    }

    template<class... C>
    void test(C&&... c)
    {
        auto data = write<C...>(std::forward<C>(c)...);
        CHECK(read<C...>(data, std::forward<C>(c)...));
    }
};

TEST(SerDes, Void) { test(); }
TEST(SerDes, Int) { test(int(1)); }
TEST(SerDes, Ints) { test(int(1), int(2)); }
TEST(SerDes, Mixed) { test(int(3), short(4), (unsigned char)5); }
TEST(SerDes, Pair) { test(std::make_pair(char(6), long(7))); }
TEST(SerDes, Tuple) { test(std::make_tuple(int(8), short(9), char(10))); }
TEST(SerDes, IntList) { test(std::list<int>{11, 12, 13}); }
TEST(SerDes, ULongDeque) { test(std::deque<unsigned long>{123456ul, 234567ul, 3456789ul}); }
TEST(SerDes, UshortForwardList) { test(std::forward_list<unsigned short>{123, 231, 312}); }
TEST(SerDes, ShortVector) { test(std::vector<short>{14, 15}); }
TEST(SerDes, EmptyLongVector) { test(std::vector<long>{}); }
TEST(SerDes, String) { test(std::string("Hi!")); }
TEST(SerDes, CharSet) { test(std::set<char>{'a', 'b', 'c'}); }
TEST(SerDes, CharMultiSet) { test(std::set<char>{'x', 'x', 'x'}); }
TEST(SerDes, IntToCharMap) { test(std::map<int, char>{{1, 'a'}, {2, 'b'}, {3, 'c'}, {4, 'a'}}); }
TEST(SerDes, CharToIntMultimap) { test(std::multimap<char, int>{{'a', 1}, {'b', 2}, {'c', 3}, {'a', 4}}); }
TEST(SerDes, CharUnorderedSet) { test(std::unordered_set<char>{'a', 'b', 'c'}); }
TEST(SerDes, CharUnorderedMultiSet) { test(std::unordered_set<char>{'x', 'x', 'x'}); }
TEST(SerDes, IntToCharUnorderedMap) { test(std::unordered_map<int, char>{{1, 'a'}, {2, 'b'}, {3, 'c'}, {4, 'a'}}); }
TEST(SerDes, CharToIntUnorderedMultimap) { test(std::unordered_multimap<char, int>{{'a', 1}, {'b', 2}, {'c', 3}, {'a', 4}}); }

TEST(SerDes, MultilevelDocumentStructure) {
    test(std::tuple<std::set<std::string>, std::map<std::pair<std::string, std::string>, int>, std::list<std::vector<std::string>>> (
        {"alpha", "beta", "delta", "gamma", "epsilon"},
        {
            {{"alpha", "beta"}, 1},
            {{"beta", "gamma"}, 2},
            {{"alpha", "delta"}, 3},
            {{"delta", "gamma"}, 4},
            {{"gamma", "epsilon"}, 5}
        },
        {
            {"alpha", "beta", "gamma", "beta", "alpha"},
            {"alpha", "delta", "gamma", "epsilon"}
        }
    ));
}

struct CustomDataRw
{
    int x = 0;
    CustomDataRw() = default;
    CustomDataRw(int x): x(x) {}
    bool operator ==(const CustomDataRw&o) const { return x == o.x; }
};

template<> struct RpcTypeInfo<CustomDataRw>
{
    static constexpr inline size_t size(const CustomDataRw& v) {
        return sizeof(int);
    }

    template<class S> static constexpr inline bool write(S &s, const CustomDataRw& v) {
        return s.write(~v.x);
    }

    template<class S> static constexpr inline bool read(S &s, CustomDataRw& v) 
    {
        if(s.read(v.x))
        {
            v.x = ~v.x;
            return true;
        }

        return false;
    }
};

TEST(SerDes, CustomDataRw) 
{
    test(CustomDataRw(123));

    MockStream stream(0);
    CHECK(!read(stream, CustomDataRw(123)));
}

struct CustomDataRo
{
    unsigned long long x = 0;
    CustomDataRo() = default;
    CustomDataRo(unsigned long long x): x(x) {}
    bool operator ==(const CustomDataRo&o) const { return x == o.x; }
};

template<> struct RpcTypeInfo<CustomDataRo>
{
    static constexpr inline size_t size(const CustomDataRo& v) { return sizeof(v.x); }
    template<class S> static constexpr inline bool read(S &s, CustomDataRo& v)  { return s.read(v.x); }
};

struct CustomDataWo
{
    unsigned long long x = 0;
    CustomDataWo() = default;
    CustomDataWo(unsigned long long x): x(x) {}
    bool operator ==(const CustomDataWo&o) const { return x == o.x; }
};

template<> struct RpcTypeInfo<CustomDataWo>
{
    static constexpr inline size_t size(const CustomDataWo& v) { return sizeof(v.x); }
    template<class S> static constexpr inline bool write(S &s, const CustomDataWo& v) { return s.write(v.x); }
};

TEST(SerDes, CustomDataDissimilarSingleSided) 
{
    auto data = write(CustomDataWo{420});
    CHECK(read(data, CustomDataRo{420}));
}

TEST(SerDes, CustomSingleSidedNested) 
{
    auto data = write(std::forward_list<CustomDataWo>{1, 4, 1, 4, 2, 1, 3, 5, 6, 2});
    CHECK(read(data, std::deque<CustomDataRo>{1, 4, 1, 4, 2, 1, 3, 5, 6, 2}));
}

TEST(SerDes, ListAsVector) 
{
    auto data = write(std::list<int>{1, 2, 3});
    CHECK(read(data, std::vector<int>{1, 2, 3}));
}

TEST(SerDes, TupleAsPair) 
{
    auto data = write(std::tuple<char, int>('a', 1));
    CHECK(read(data, std::pair<char, int>('a', 1)));
}

TEST(SerDes, StringIntHashMapAsVectorOfPairs) 
{
    auto data = write(std::map<std::string, int>{{"foo", 42}, {"bar", 69}});
    CHECK(read(data, std::vector<std::pair<std::string, int>>{{"bar", 69}, {"foo", 42}}));
}

TEST(SerDes, LongStrings) 
{
    test(std::string(128, '.'));
    test(std::string(128*128, '.'));
    test(std::string(128*128*128, '.'));
    // Takes too long: test(std::string(128*128*128*128, '.'));
}

TEST(SerDes, Truncate) 
{
    for(int i=0; ; i++)
    {
        auto data = write(std::string("abc"));

        if(!data.truncateAt(i))
        {
            CHECK(read(data, std::forward_list<char>{'a', 'b', 'c'}));
            break;
        }

        CHECK(!read(data, std::forward_list<char>{'a', 'b', 'c'}));
    }
}

TEST(SerDes, NoSpace)
{
    auto data = std::string("panzerkampfwagen");
    auto s = determineSize(data);

    for(int i = 0; i <= s; i++)
    {
        MockStream stream(i);
        auto a = stream.access();
        bool sok = serialize(a, data);

        if(i < s)
            CHECK(!sok);
        else
            CHECK(sok);
    }
}

TEST(SerDes, VarUint4) 
{
    uint32_t ns[] = {0, 1, 127, 128, 129, 
        128*128-1, 128*128, 128*128+1, 
        128*128*128-1, 128*128*128, 128*128*128+1, 
        128*128*128*128-1, 128*128*128*128, 128*128*128*128+1};

    for(auto n: ns)
    {
        const auto s = VarUint4::size(n);

        for(int i = 0; i < s; i++)
        {
            MockStream stream(i);
            auto a = stream.access();
            CHECK(!VarUint4::write(a, n));
        }

        MockStream stream(s);
        auto a = stream.access();
        CHECK(VarUint4::write(a, n));
        CHECK(!a.write('\0'));

        uint32_t r;
        auto b = stream.access();
        CHECK(VarUint4::read(b, r));
        CHECK(r == n);
    }
}
