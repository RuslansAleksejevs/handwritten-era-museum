#include "algorithms.hpp"
#include "async_search.hpp"
#include "concurrency.hpp"
#include "foundations.hpp"
#include "services.hpp"
#include <chrono>
#include <iostream>
#include <numeric>
#include <random>
#include <streambuf>
namespace {
using namespace museum::red;
void Check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void Throws(F f) {
    bool thrown = false;
    try {
        f();
    } catch (const std::exception &) {
        thrown = true;
    }
    Check(thrown, "expected exception");
}
struct MoveOnly {
    int value;
    explicit MoveOnly(int n) : value(n) {}
    MoveOnly(const MoveOnly &) = delete;
    MoveOnly &operator=(const MoveOnly &) = delete;
    MoveOnly(MoveOnly &&) = default;
    MoveOnly &operator=(MoveOnly &&) = default;
    bool operator<(const MoveOnly &other) const { return value < other.value; }
    bool IsEndSentencePunctuation() const { return value < 0; }
};
struct HeavyChar {
    static int copies;
    char value;
    HeavyChar(char c) : value(c) {}
    HeavyChar(const HeavyChar &c) : value(c.value) { ++copies; }
    HeavyChar &operator=(const HeavyChar &c) {
        value = c.value;
        ++copies;
        return *this;
    }
    HeavyChar(HeavyChar &&) = default;
    HeavyChar &operator=(HeavyChar &&) = default;
    bool operator<(const HeavyChar &c) const { return value < c.value; }
    bool operator==(const HeavyChar &c) const { return value == c.value; }
};
int HeavyChar::copies = 0;
struct Lifetime {
    static int alive, moves, throw_on_move;
    int value = 0;
    Lifetime() { ++alive; }
    explicit Lifetime(int n) : value(n) { ++alive; }
    Lifetime(const Lifetime &other) : value(other.value) { ++alive; }
    Lifetime(Lifetime &&other) : value(other.value) {
        if (++moves == throw_on_move)
            throw std::runtime_error("injected move failure");
        ++alive;
    }
    ~Lifetime() { --alive; }
};
int Lifetime::alive = 0;
int Lifetime::moves = 0;
int Lifetime::throw_on_move = -1;
void Foundations() {
    std::vector<int> numbers{8, 9, 9, 2};
    Check(max_element_if(numbers.begin(), numbers.end(), [](int n) { return n % 2; }) ==
              numbers.begin() + 1,
          "predicate max must use first tie");
    Check(max_element_if(numbers.begin(), numbers.end(), [](int) { return false; }) ==
              numbers.end(),
          "no qualifying max");
    std::ostringstream logs;
    Logger logger(logs);
    LOG(logger, "plain");
    logger.SetLogLine(true);
    logger.Log("line", "file", 7);
    logger.SetLogFile(true);
    logger.Log("both", "file", 8);
    Check(logs.str() == "plain\n7: line\nfile:8: both\n", "logger options");
    AirlineTicket ticket;
    ticket.price = 100;
    UPDATE_FIELD(ticket, price, (std::map<std::string, std::string>{{"price", "25"}}));
    UPDATE_FIELD(ticket, departure_date,
                 (std::map<std::string, std::string>{{"departure_date", "2020-02-29"}}));
    Check(ticket.price == 25 && ticket.departure_date == Date{2020, 2, 29}, "ticket field update");
    std::vector<AirlineTicket> tickets(2);
    tickets[0].price = 3;
    tickets[1].price = 1;
    std::sort(tickets.begin(), tickets.end(), SORT_BY(price));
    Check(tickets.front().price == 1, "comparator macro");
    std::ostringstream printed;
    if (true)
        PRINT_VALUES(printed, 1, 2);
    else
        printed << "wrong";
    Check(printed.str() == "1\n2\n", "safe statement macro");
    [[maybe_unused]] int UNIQ_ID = 1;
    [[maybe_unused]] std::string UNIQ_ID = "unique";
    Table<int> table(2, 3);
    table[1][2] = 9;
    table.Resize(3, 4);
    Check(table[1][2] == 9 && table[2][3] == 0, "table preserve and initialize");
    table.Resize(0, 4);
    Check(table.Size() == std::make_pair(std::size_t{0}, std::size_t{4}), "empty table dimensions");
    Deque<int> deque;
    deque.PushFront(2);
    deque.PushFront(1);
    deque.PushBack(3);
    deque.At(1) = 8;
    const auto &cd = deque;
    Check(cd.Front() == 1 && cd[1] == 8 && cd.Back() == 3, "two-vector deque");
    Throws([&] { deque.At(3); });
    std::list<int> items{1, 2, 3, 4, 5};
    auto pages = Paginate(items, 2);
    Check(pages.size() == 3, "forward iterator pagination");
    for (auto page : pages)
        for (int &value : page)
            value *= 2;
    Check(items.back() == 10, "pages borrow mutable elements");
    Throws([&] { Paginate(items, 0); });
    Student a{"a", "", {}, 5}, b{"b", "", {}, 2};
    Check(Compare(a, b) && !Compare(b, a), "rating compare");
    Learner learner;
    Check(learner.Learn({"b", "a", "a"}) == 2 && learner.Learn({"a", "c"}) == 1 &&
              learner.KnownWords() == std::vector<std::string>{"a", "b", "c"},
          "learner uniqueness");
    StackVector<int, 2> stack;
    stack.PushBack(1);
    stack.PushBack(2);
    Throws([&] { stack.PushBack(3); });
    Check(stack.PopBack() == 2, "stack pop");
    stack.PopBack();
    Throws([&] { stack.PopBack(); });
    Throws([] { StackVector<int, 0> bad(1); });
    SimpleVector<int> vector;
    for (int i = 0; i < 100; ++i)
        vector.PushBack(i);
    auto copy = vector;
    auto *self = &copy;
    copy = *self;
    copy[0] = 20;
    Check(vector[0] == 0 && copy.Size() == 100 && copy.Capacity() >= 100,
          "deep copy and self assignment");
    SimpleVector<int> alias;
    alias.PushBack(9);
    alias.PushBack(alias[0]);
    Check(alias[1] == 9, "aliased append before reallocation");
    SimpleVector<std::unique_ptr<int>> movable;
    for (int i = 0; i < 30; ++i)
        movable.PushBack(std::make_unique<int>(i));
    auto moved = std::move(movable);
    Check(movable.Size() == 0 && *moved[29] == 29, "move-only vector growth");
}
void ContainersAndMoves() {
    {
        ObjectPool<Lifetime> tracked;
        auto *first = tracked.Allocate();
        tracked.Allocate();
        tracked.Deallocate(first);
        Check(Lifetime::alive == 2, "pool owns allocated and free objects");
    }
    Check(Lifetime::alive == 0, "pool destroys both sets");
    {
        SimpleVector<Lifetime> vector;
        vector.PushBack(Lifetime(1));
        Lifetime::throw_on_move = Lifetime::moves + 1;
        Throws([&] { vector.PushBack(Lifetime(2)); });
        Lifetime::throw_on_move = -1;
        Check(vector.Size() == 1 && Lifetime::alive == 1,
              "failed vector relocation cleans temporary storage");
        vector.PushBack(Lifetime(3));
        Check(vector.Size() == 2, "vector reusable after failed relocation");
    }
    Check(Lifetime::alive == 0, "vector destroys live elements");
    ObjectPool<std::string> pool;
    auto a = pool.Allocate(), b = pool.Allocate();
    *a = "keep";
    pool.Deallocate(a);
    pool.Deallocate(b);
    Check(pool.TryAllocate() == a && *a == "keep" && pool.TryAllocate() == b && !pool.TryAllocate(),
          "FIFO pool reuse");
    Throws([&] { pool.Deallocate(nullptr); });
    int x = 1, y = 2;
    Swap(&x, &y);
    Check(x == 2 && y == 1, "pointer swap");
    std::vector<int *> pointers{&x, &y};
    SortPointers(pointers);
    Check(*pointers[0] == 1, "pointer sorting");
    for (int source = 0; source < 8; ++source)
        for (int dest = 0; dest < 8; ++dest)
            for (int n = 0; n <= 8; ++n) {
                std::vector<int> memory(20);
                std::iota(memory.begin(), memory.end(), 0);
                auto expected = memory;
                for (int i = 0; i < n; ++i)
                    expected[dest + i] = memory[source + n - 1 - i];
                ReversedCopy(memory.data() + source, n, memory.data() + dest);
                Check(memory == expected, "overlapping reversed copy");
            }
    std::vector<HeavyChar> copied;
    for (char c : std::string("abcdefgh"))
        copied.emplace_back(c);
    HeavyChar::copies = 0;
    ReversedCopy(copied.data(), 6, copied.data() + 2);
    Check(HeavyChar::copies <= 6, "overlapping copy does not recopy source values");
    LinkedList<int> list;
    list.InsertAfter(nullptr, 1);
    list.InsertAfter(list.GetHead(), 2);
    list.RemoveAfter(list.GetHead());
    Check(!list.GetHead()->next, "linked erase tail");
    list.RemoveAfter(nullptr);
    list.PopFront();
    Check(!list.GetHead(), "linked empty");
    Translator translator;
    {
        std::string source = "a", target = "one";
        translator.Add(source, target);
    }
    translator.Add("a", "two");
    translator.Add("b", "two");
    Check(translator.TranslateForward("a") == "two" && translator.TranslateBackward("one") == "a" &&
              translator.TranslateBackward("two") == "b" &&
              translator.TranslateForward("missing").empty(),
          "translator stable storage and independent last pairs");
    enum class Airport { A, B, Last_ };
    AirportCounter<Airport> flights;
    flights.Insert(Airport::B);
    flights.EraseOne(Airport::A);
    auto counts = flights.GetItems();
    Check(counts.size() == 2 && counts[0].second == 0 && counts[1].second == 1,
          "airport enum counters");
    flights.EraseAll(Airport::B);
    Check(!flights.Get(Airport::B), "airport clear");
    Editor editor;
    for (char c : std::string("hello, world"))
        editor.Insert(c);
    for (int i = 0; i < 12; ++i)
        editor.Left();
    editor.Cut(7);
    for (int i = 0; i < 5; ++i)
        editor.Right();
    editor.Insert(',');
    editor.Insert(' ');
    editor.Paste();
    editor.Left();
    editor.Left();
    editor.Cut(3);
    Check(editor.GetText() == "world, hello", "editor example");
    editor.Copy(0);
    editor.Paste();
    Check(editor.GetText() == "world, hello", "empty copy clears clipboard");
    web::Stats stats;
    std::string line = "  GET /order HTTP/1.1";
    auto req = web::ParseRequest(line);
    stats.AddMethod(req.method);
    stats.AddUri(req.uri);
    stats.AddMethod("PATCH");
    stats.AddUri("/missing");
    Check(stats.GetMethodStats().at("GET") == 1 && stats.GetMethodStats().at("UNKNOWN") == 1 &&
              stats.GetUriStats().at("unknown") == 1,
          "web parser and buckets");
    std::vector<MoveOnly> josephus;
    for (int i = 0; i < 10; ++i)
        josephus.emplace_back(i);
    MakeJosephusPermutation(josephus.begin(), josephus.end(), 3);
    const std::vector<int> expected{0, 3, 6, 9, 4, 8, 5, 2, 7, 1};
    for (int i = 0; i < 10; ++i)
        Check(josephus[i].value == expected[i], "Josephus move-only order");
    std::vector<std::vector<HeavyChar>> heavy(3);
    for (char c : std::string("law"))
        heavy[0].emplace_back(c);
    for (char c : std::string("wall"))
        heavy[1].emplace_back(c);
    heavy[2].emplace_back('x');
    HeavyChar::copies = 0;
    auto groups = GroupHeavyStrings(std::move(heavy));
    Check(groups.size() == 2 && groups[0].size() == 2 && HeavyChar::copies <= 8,
          "heavy chars copied once per input char");
    std::vector<MoveOnly> tokens;
    for (int i : {1, -1, -2, 2, 3})
        tokens.emplace_back(i);
    auto sentences = SplitIntoSentences(std::move(tokens));
    Check(sentences.size() == 2 && sentences[0].size() == 3 && sentences[1].size() == 2,
          "sentence punctuation runs");
    std::vector<MoveOnly> sortable;
    for (int i : {9, 1, 7, 2, 3, 6, 4, 8, 5})
        sortable.emplace_back(i);
    MergeSort(sortable.begin(), sortable.end());
    for (int i = 0; i < 9; ++i)
        Check(sortable[i].value == i + 1, "move-only three-way mergesort");
    PriorityCollection<std::unique_ptr<int>> priorities;
    auto first = priorities.Add(std::make_unique<int>(1));
    auto second = priorities.Add(std::make_unique<int>(2));
    Check(*priorities.GetMax().first == 2, "latest priority tie");
    priorities.Promote(first);
    Check(*priorities.PopMax().first == 1 && !priorities.IsValid(first) &&
              priorities.IsValid(second),
          "priority update and invalidation");
}
void ServicesAndOracles() {
    std::mt19937 random(17);
    Express express;
    std::map<int, std::vector<int>> connections;
    for (int i = 0; i < 3000; ++i) {
        int a = static_cast<int>(random() % 100) - 50, b = static_cast<int>(random() % 100) - 50;
        if (i % 3 == 0) {
            express.Add(a, b);
            connections[a].push_back(b);
            connections[b].push_back(a);
        } else {
            long long expected = std::abs(a - b);
            for (int stop : connections[a])
                expected = std::min(expected, static_cast<long long>(std::abs(stop - b)));
            Check(express.Go(a, b) == expected, "express nearest-endpoint oracle");
        }
    }
    ReadingManager reading;
    std::map<int, int> pages;
    for (int i = 0; i < 5000; ++i) {
        int user = 1 + random() % 50;
        if (i % 3 == 0) {
            int page = 1 + random() % 1000;
            pages[user] = page;
            reading.Read(user, page);
        } else {
            double expected = 0;
            if (pages.count(user)) {
                if (pages.size() == 1)
                    expected = 1;
                else {
                    int lower = 0;
                    for (auto [id, page] : pages)
                        lower += page < pages[user];
                    expected = static_cast<double>(lower) / (pages.size() - 1);
                }
            }
            Check(std::abs(reading.Cheer(user) - expected) < 1e-12, "reading naive oracle");
        }
    }
    BookingManager bookings;
    bookings.Book(-100000, "hotel", 1, 2);
    bookings.Book(-13601, "hotel", 1, 3);
    Check(bookings.Clients("hotel") == 1 && bookings.Rooms("hotel") == 5,
          "duplicate client window");
    bookings.Book(-13600, "other", 2, 1);
    Check(bookings.Rooms("hotel") == 3, "exclusive lower booking boundary");
    bookings.Book(1000000000000000000LL, "other", 3, 4);
    Check(bookings.Clients("hotel") == 0, "large timestamp eviction");
    Check(Sportsmen({{42, 0}, {17, 42}, {13, 0}, {123, 42}, {5, 13}}) ==
              std::vector<int>{17, 123, 42, 5, 13},
          "sportsmen order");
}
class BlockingInput : public std::streambuf {
    std::promise<void> &entered_;
    std::shared_future<void> released_;
    std::string text_ = "new\n";
    bool read_ = false;
    int_type underflow() override {
        if (read_)
            return traits_type::eof();
        read_ = true;
        entered_.set_value();
        released_.wait();
        setg(text_.data(), text_.data(), text_.data() + text_.size());
        return traits_type::to_int_type(*gptr());
    }

  public:
    BlockingInput(std::promise<void> &entered, std::shared_future<void> released)
        : entered_(entered), released_(std::move(released)) {}
};
class SignalledOutput : public std::streambuf {
    std::promise<void> &done_;
    int_type overflow(int_type ch) override {
        if (!traits_type::eq_int_type(ch, traits_type::eof())) {
            char c = traits_type::to_char_type(ch);
            text += c;
            if (c == '\n')
                done_.set_value();
        }
        return ch;
    }

  public:
    std::string text;
    explicit SignalledOutput(std::promise<void> &done) : done_(done) {}
};
void Concurrency() {
    Synchronized<int> value;
    ConcurrentMap<int, int> counts(7);
    std::vector<std::future<void>> workers;
    for (int thread = 0; thread < 4; ++thread)
        workers.push_back(std::async(std::launch::async, [&] {
            for (int i = 0; i < 10000; ++i) {
                ++value.GetAccess().ref_to_value;
                ++counts[i % 31 - 15].ref_to_value;
            }
        }));
    for (auto &worker : workers)
        worker.get();
    Check(value.GetAccess().ref_to_value == 40000, "synchronized increments");
    int total = 0;
    for (auto [key, n] : counts.BuildOrdinaryMap())
        total += n;
    Check(total == 40000, "concurrent map signed keys");
    std::vector<std::vector<int>> matrix(100, std::vector<int>(100, 1000000000));
    Check(CalculateMatrixSum(matrix) == 10000000000000LL, "parallel sum uses int64");
    std::istringstream posts("yangle rocks sucks, really\nyangle yangle\n");
    auto stats = keywords::ExploreKeyWords({"yangle", "rocks", "sucks", "all"}, posts);
    Check(stats.word_frequences == std::map<std::string, int>{{"rocks", 1}, {"yangle", 3}},
          "parallel keywords exact tokens");
    std::istringstream original("old\n"), queries("old new\n");
    std::promise<void> entered, released, query_done;
    auto entered_future = entered.get_future();
    auto done_future = query_done.get_future();
    BlockingInput buffer(entered, released.get_future().share());
    std::istream replacement(&buffer);
    SignalledOutput output_buffer(query_done);
    std::ostream output(&output_buffer);
    AsyncSearchServer server(original);
    server.UpdateDocumentBase(replacement);
    const bool started =
        entered_future.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    server.AddQueriesStream(queries, output);
    const bool progressed =
        done_future.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    released.set_value();
    server.Wait();
    Check(started && progressed && output_buffer.text == "old new: {docid: 0, hitcount: 1}\n",
          "async query progresses during blocked update");
}
} // namespace
int main() {
    try {
        Foundations();
        ContainersAndMoves();
        ServicesAndOracles();
        Concurrency();
        std::cout << "PASS red: all 37 mapped statement pages; examples, move-only cases, 3000 "
                     "express/5000 reading oracle operations, 40000 concurrent increments, "
                     "asynchronous progress\n";
    } catch (const std::exception &e) {
        std::cerr << "FAIL red: " << e.what() << '\n';
        return 1;
    }
}
