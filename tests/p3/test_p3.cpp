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

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
