// tests/p3/test_p3.cpp — YOUR P3 TEST SUITE.
//
// The five tests below are worked examples. Keep them, and add your own
// until you have AT LEAST 15 (spec §6 lists what must be covered).
// Name every test <area>_<what>: reg_ for the registry, ws_ for the
// workspace, calc_ for the calculator.
//
// Build & run:   cmake --build build && ./build/test_p3
// One area:      ./build/test_p3 reg_        (names starting with "reg_")
// One test:      ./build/test_p3 =reg_add_and_find

#include "../mini_test.h"
#include "../test_helpers.h"     // TempDir, write_text, args1, ...

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A tool whose name you choose — use it to fill the registry in tests.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;   // an empty JSON object: {}
};

// Test-only tool: record when its destructor runs.
class CountingTool : public NamedTool {
public:
    CountingTool(std::string name, int& destructions)
        : NamedTool(std::move(name)), destructions_(destructions) {}
    ~CountingTool() override { ++destructions_; }
private:
    int& destructions_;
};

// Tokenize with the provided tokenizer, then run YOUR calc_evaluate.
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

}  // namespace

// ---------------------------------------------------------------- examples

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();                        // remember the address before moving
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);              // same object, not a copy
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));   // sanity: accepts *something*
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
}

// ------------------------------------------------------------- your tests
//
// Still to write (spec §6 has the full list). Suggested names:
//   reg_null_rejected, reg_for_each_visits_each_once
//   reg_10000_tools               REQUIRED: print bucket_count / load_factor /
//                                 max_probe_length, check the bound
//   reg_alternative_hash          REQUIRED: longest chain of a weak hash
//   ws_<case>                     REQUIRED: every row of the table in spec §4.4
//   calc_<case>                   left-to-right, parentheses, negatives

TEST(reg_resize_preserves_tools) {
    ToolRegistry r;
    const std::size_t initial = r.bucket_count();
    std::vector<Tool*> original_pointers;

    // Cross two resize boundaries: initial -> 2*initial -> 4*initial.
    for (std::size_t i = 0; i <= initial * 2; ++i) {
        const std::string name = "resize_" + std::to_string(i);
        auto tool = std::make_unique<NamedTool>(name);
        original_pointers.push_back(tool.get());
        REQUIRE(r.add(std::move(tool)));
        CHECK(r.size() == i + 1);
        CHECK(r.load_factor() <= 1.0);
        if (i + 1 == initial) {
            CHECK(r.bucket_count() == initial);
            CHECK(r.load_factor() == 1.0);
            CHECK(!r.add(std::make_unique<NamedTool>("resize_0")));
            CHECK(r.bucket_count() == initial); // A duplicate must not grow the table.
            CHECK(r.size() == initial);
        }
        if (i == initial) CHECK(r.bucket_count() == initial * 2);
    }

    CHECK(r.bucket_count() == initial * 4);
    for (std::size_t i = 0; i < original_pointers.size(); ++i) {
        CHECK(r.find("resize_" + std::to_string(i)) == original_pointers[i]);
    }
    CHECK(r.find("missing") == nullptr);
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    std::size_t calls = 0;
    r.for_each([&](const Tool&) { ++calls; });
    CHECK(calls == 0); // An empty registry makes no calls.

    // Enough tools to exercise iteration after a resize too.
    const std::size_t count = r.bucket_count() + 1;
    std::vector<Tool*> originals;
    std::vector<std::string> visited;
    for (std::size_t i = 0; i < count; ++i) {
        auto tool = std::make_unique<NamedTool>("visit_" + std::to_string(i));
        originals.push_back(tool.get());
        REQUIRE(r.add(std::move(tool)));
    }
    r.for_each([&](const Tool& tool) {
        ++calls;
        visited.push_back(tool.name());
        CHECK(r.find(tool.name()) == &tool); // Callback gets the stored object.
    });
    CHECK(calls == count);
    CHECK(r.size() == count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::string name = "visit_" + std::to_string(i);
        std::size_t occurrences = 0;
        for (const auto& seen : visited) {
            if (seen == name) ++occurrences;
        }
        CHECK(occurrences == 1); // No promised iteration order.
        CHECK(r.find(name) == originals[i]); // Iteration did not take ownership.
    }
}

TEST(reg_max_probe_length_collisions) {
    ToolRegistry r;
    CHECK(r.max_probe_length() == 0);
    const std::size_t buckets = r.bucket_count();
    const std::size_t target = ToolRegistry::hash("collision_0") % buckets;
    std::vector<std::string> names;

    // Find three distinct names that share a bucket without triggering a resize.
    for (std::size_t i = 0; i < 10000 && names.size() < 3; ++i) {
        const std::string name = "collision_" + std::to_string(i);
        if (ToolRegistry::hash(name) % buckets == target) {
            names.push_back(name);
        }
    }
    REQUIRE(names.size() == 3);
    for (std::size_t i = 0; i < names.size(); ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>(names[i])));
        CHECK(r.max_probe_length() == i + 1);
    }
    CHECK(r.bucket_count() == buckets);
    for (const auto& name : names) CHECK(r.find(name) != nullptr);

    // A tool in another bucket must not increase the longest chain.
    std::string other = "other";
    while (ToolRegistry::hash(other) % buckets == target) other += "x";
    REQUIRE(r.add(std::make_unique<NamedTool>(other)));
    CHECK(r.max_probe_length() == 3);
}

TEST(reg_null_rejected) {
    ToolRegistry r;
    const std::size_t initial_buckets = r.bucket_count();

    // Reject a null pointer when the registry is empty.
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 0);
    CHECK(r.bucket_count() == initial_buckets);
    CHECK(r.find("alpha") == nullptr);

    // Reject it without disturbing a tool that is already registered.
    auto tool = std::make_unique<NamedTool>("alpha");
    Tool* original = tool.get();
    REQUIRE(r.add(std::move(tool)));
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 1);
    CHECK(r.bucket_count() == initial_buckets);
    CHECK(r.find("alpha") == original);
}

TEST(reg_10000_tools) {
    ToolRegistry r;
    Tool* first = nullptr;
    constexpr std::size_t count = 10000;

    for (std::size_t i = 0; i < count; ++i) {
        auto tool = std::make_unique<NamedTool>("tool_" + std::to_string(i));
        if (i == 0) first = tool.get();
        REQUIRE(r.add(std::move(tool)));
        CHECK(r.size() == i + 1);
        CHECK(r.load_factor() <= 1.0);
    }

    for (std::size_t i = 0; i < count; ++i) {
        const std::string name = "tool_" + std::to_string(i);
        Tool* found = r.find(name);
        REQUIRE(found != nullptr);
        CHECK(found->name() == name);
    }
    CHECK(r.find("tool_0") == first);
    CHECK(r.find("tool_10000") == nullptr);
    CHECK(r.size() == count);
    const std::size_t longest = r.max_probe_length();
    CHECK(longest <= 64);
    std::printf("10000 tools: buckets=%zu, load_factor=%.6f, max_probe_length=%zu\n",
                r.bucket_count(), r.load_factor(), longest);
}

TEST(reg_alternative_hash) {
    ToolRegistry r;
    constexpr std::size_t count = 10000;
    for (std::size_t i = 0; i < count; ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>("tool_" + std::to_string(i))));
    }

    // Simulate the weak hash using counts, not a second registry.
    std::vector<int> weak_counts(r.bucket_count(), 0);
    for (std::size_t i = 0; i < count; ++i) {
        const std::string name = "tool_" + std::to_string(i);
        const std::size_t weak_hash =
            static_cast<unsigned char>(name[0]) + name.size();
        ++weak_counts[weak_hash % weak_counts.size()];
    }
    int weak_longest = 0;
    std::size_t total = 0;
    for (int entries : weak_counts) {
        if (entries > weak_longest) weak_longest = entries;
        total += static_cast<std::size_t>(entries);
    }

    CHECK(total == count);
    // tool_1000 through tool_9999 all start with 't' and have length 9.
    CHECK(weak_longest == 9000);
    CHECK(r.max_probe_length() <= 64);
    std::printf("Hash comparison: tools=%zu, buckets=%zu, load_factor=%.6f, "
                "real_longest=%zu, weak_longest=%d\n",
                count, r.bucket_count(), r.load_factor(),
                r.max_probe_length(), weak_longest);
}

TEST(reg_ownership_cleanup) {
    // Counters outlive the registry; never resize this vector while tools refer to it.
    std::vector<int> destructions(33, 0);
    int duplicate_destructions = 0;
    {
        ToolRegistry r;
        const std::size_t initial_buckets = r.bucket_count();
        auto first = std::make_unique<CountingTool>("owned_0", destructions[0]);
        Tool* original = first.get();
        REQUIRE(r.add(std::move(first)));
        CHECK(first == nullptr);
        CHECK(destructions[0] == 0);

        CHECK(!r.add(std::make_unique<CountingTool>("owned_0", duplicate_destructions)));
        CHECK(duplicate_destructions == 1);
        CHECK(destructions[0] == 0);
        CHECK(r.size() == 1);
        CHECK(r.find("owned_0") == original);

        for (std::size_t i = 1; i < destructions.size(); ++i) {
            REQUIRE(r.add(std::make_unique<CountingTool>(
                "owned_" + std::to_string(i), destructions[i])));
        }
        CHECK(r.bucket_count() > initial_buckets);
        CHECK(r.find("owned_0") == original);
        for (int count : destructions) CHECK(count == 0);
    } // Leaving this scope destroys the registry and every tool it owns.

    for (int count : destructions) CHECK(count == 1);
    CHECK(duplicate_destructions == 1);
}

TEST(ws_rejects_empty_path) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    CHECK(!ws.resolve("", &out, &reason));
    CHECK(reason == "empty path");
}

TEST(ws_rejects_embedded_nul) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    const std::string path = std::string("notes") + '\0' + "/x";
    CHECK(!ws.resolve(path, &out, &reason));
    CHECK(reason == "path contains NUL");
}

TEST(ws_rejects_absolute_paths) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    CHECK(!ws.resolve("/etc/passwd", &out, &reason));
    CHECK(reason == "path must be relative to workspace");
    reason.clear();
    CHECK(!ws.resolve("/", &out, &reason));
    CHECK(reason == "path must be relative to workspace");

    // Even an absolute path pointing inside the workspace is not valid input.
    const std::string inside = (ws.root() / "notes" / "todo.txt").string();
    reason.clear();
    CHECK(!ws.resolve(inside, &out, &reason));
    CHECK(reason == "path must be relative to workspace");
}

TEST(ws_normalizes_safe_paths) {
    TempDir d;
    Workspace ws(d.path());
    struct Case { const char* input; const char* relative; };
    const Case cases[] = {
        {"notes/todo.txt", "notes/todo.txt"},
        {"./notes//todo.txt", "notes/todo.txt"},
        {"a/../b", "b"},
        {"my..notes.txt", "my..notes.txt"}
    };
    for (const auto& c : cases) {
        fs::path out;
        std::string reason = "previous error";
        REQUIRE(ws.resolve(c.input, &out, &reason));
        CHECK(out == ws.root() / c.relative);
        CHECK(reason.empty());
    }
}

TEST(ws_normalizes_to_root) {
    TempDir d;
    Workspace ws(d.path());
    for (const char* input : {".", "a/.."}) {
        fs::path out;
        std::string reason;
        REQUIRE(ws.resolve(input, &out, &reason));
        CHECK(out == ws.root());
    }
}

TEST(ws_accepts_nonexistent_path_without_creating_it) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("new/dir/file.txt", &out, &reason));
    CHECK(out == ws.root() / "new/dir/file.txt");
    CHECK(!fs::exists(ws.root() / "new")); // Only the test inspects the filesystem.
}

TEST(ws_rejects_normalized_parent_escapes) {
    TempDir d;
    Workspace ws(d.path() / "ws");
    for (const char* input : {"..", "../etc/passwd", "a/../../b",
                              "notes/../../ws/notes/todo.txt"}) {
        fs::path out;
        std::string reason;
        CHECK(!ws.resolve(input, &out, &reason));
        CHECK(reason == "path escapes workspace");
    }
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
