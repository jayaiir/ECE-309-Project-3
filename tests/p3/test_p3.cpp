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

#include <algorithm>
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

// ------------------------------------------------------------- registry

namespace {

// Counts how many times a tool is destroyed, to check ownership.
class CountingTool : public NamedTool {
public:
    CountingTool(std::string n, int* deaths) : NamedTool(std::move(n)), deaths_(deaths) {}
    ~CountingTool() override { ++*deaths_; }
private:
    int* deaths_;
};

std::string tool_name(int i) { return "tool_" + std::to_string(i); }

}  // namespace

TEST(reg_empty_table) {
    ToolRegistry r;
    CHECK_EQ(r.size(), std::size_t{0});
    CHECK(r.bucket_count() >= 1 && r.bucket_count() <= 64);
    CHECK(r.load_factor() == 0.0);
    CHECK_EQ(r.max_probe_length(), std::size_t{0});
    CHECK(r.find("anything") == nullptr);
    CHECK(r.find("") == nullptr);
}

TEST(reg_null_rejected) {
    ToolRegistry r;
    CHECK(!r.add(nullptr));
    CHECK_EQ(r.size(), std::size_t{0});
    REQUIRE(r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(!r.add(std::unique_ptr<Tool>{}));
    CHECK_EQ(r.size(), std::size_t{1});
}

TEST(reg_find_absent) {
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<NamedTool>("read_file")));
    CHECK(r.find("teleport") == nullptr);
    CHECK(r.find("read_fil") == nullptr);    // prefix is not a match
    CHECK(r.find("read_file2") == nullptr);
    CHECK(r.find("READ_FILE") == nullptr);   // case-sensitive
    CHECK_EQ(r.size(), std::size_t{1});
}

TEST(reg_ownership_destroys_once) {
    int deaths = 0;
    {
        ToolRegistry r;
        REQUIRE(r.add(std::make_unique<CountingTool>("a", &deaths)));
        REQUIRE(r.add(std::make_unique<CountingTool>("b", &deaths)));
        // The duplicate is destroyed immediately; the original survives.
        CHECK(!r.add(std::make_unique<CountingTool>("a", &deaths)));
        CHECK_EQ(deaths, 1);
        for (int i = 0; i < 100; ++i)   // force several resizes
            REQUIRE(r.add(std::make_unique<CountingTool>(tool_name(i), &deaths)));
        CHECK_EQ(deaths, 1);            // a rehash must not destroy anything
    }
    CHECK_EQ(deaths, 103);              // 102 owned + 1 duplicate, each exactly once
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    const std::vector<std::string> names = {"read_file", "write_file", "list_dir",
                                            "calculator", "alpha", "beta", "gamma",
                                            "delta", "epsilon", "zeta", "eta"};
    for (const std::string& n : names) REQUIRE(r.add(std::make_unique<NamedTool>(n)));

    std::vector<int> seen(names.size(), 0);
    int count = 0;
    r.for_each([&](const Tool& t) {
        ++count;
        for (std::size_t i = 0; i < names.size(); ++i)
            if (names[i] == t.name()) ++seen[i];
    });
    CHECK_EQ(count, static_cast<int>(names.size()));
    for (int s : seen) CHECK_EQ(s, 1);

    ToolRegistry empty;
    int calls = 0;
    empty.for_each([&](const Tool&) { ++calls; });
    CHECK_EQ(calls, 0);
}

TEST(reg_hash_properties) {
    CHECK(ToolRegistry::hash("ab") != ToolRegistry::hash("ba"));
    CHECK(ToolRegistry::hash("tool_1234") != ToolRegistry::hash("tool_1243"));
    CHECK(ToolRegistry::hash("abc") == ToolRegistry::hash("abc"));
    // Every character matters, including the last one.
    CHECK(ToolRegistry::hash("calculator") != ToolRegistry::hash("calculatoR"));

    std::vector<std::size_t> hs;
    for (int i = 0; i < 10000; ++i) hs.push_back(ToolRegistry::hash(tool_name(i)));
    std::sort(hs.begin(), hs.end());
    std::size_t distinct = static_cast<std::size_t>(
        std::unique(hs.begin(), hs.end()) - hs.begin());
    CHECK(distinct >= 9990);
}

TEST(reg_10000_tools) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>(tool_name(0));
    Tool* first_ptr = first.get();
    REQUIRE(r.add(std::move(first)));
    REQUIRE(r.find(tool_name(0)) == first_ptr);

    bool lf_ok = r.load_factor() <= 1.0;
    std::size_t resizes = 0, last_buckets = r.bucket_count();
    for (int i = 1; i < 10000; ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>(tool_name(i))));
        if (r.load_factor() > 1.0) lf_ok = false;
        if (r.bucket_count() != last_buckets) { ++resizes; last_buckets = r.bucket_count(); }
    }
    CHECK(lf_ok);
    CHECK_EQ(r.size(), std::size_t{10000});
    CHECK(resizes > 0);

    // Pointer from the very first insert survived every resize.
    CHECK(r.find(tool_name(0)) == first_ptr);
    CHECK(first_ptr->name() == tool_name(0));

    int missing = 0;
    for (int i = 0; i < 10000; ++i) {
        Tool* t = r.find(tool_name(i));
        if (t == nullptr || t->name() != tool_name(i)) ++missing;
    }
    CHECK_EQ(missing, 0);
    CHECK(r.find("tool_10000") == nullptr);
    CHECK(r.max_probe_length() <= 64);

    std::printf("    [FNV-1a, chaining] bucket_count=%zu load_factor=%.4f "
                "max_probe_length=%zu resizes=%zu\n",
                r.bucket_count(), r.load_factor(), r.max_probe_length(), resizes);
}

TEST(reg_alternative_hash) {
    ToolRegistry r;
    for (int i = 0; i < 10000; ++i) REQUIRE(r.add(std::make_unique<NamedTool>(tool_name(i))));
    const std::size_t buckets = r.bucket_count();

    // Longest chain the weak hash s[0] + s.size() would produce at the same size.
    std::vector<int> weak(buckets, 0);
    for (int i = 0; i < 10000; ++i) {
        std::string s = tool_name(i);
        std::size_t h = static_cast<unsigned char>(s[0]) + s.size();
        ++weak[h & (buckets - 1)];
    }
    int weak_longest = *std::max_element(weak.begin(), weak.end());
    std::size_t weak_used = static_cast<std::size_t>(
        std::count_if(weak.begin(), weak.end(), [](int c) { return c > 0; }));

    // djb2 for comparison.
    std::vector<int> djb(buckets, 0);
    for (int i = 0; i < 10000; ++i) {
        std::size_t h = 5381;
        for (unsigned char c : tool_name(i)) h = h * 33 + c;
        ++djb[h & (buckets - 1)];
    }
    int djb_longest = *std::max_element(djb.begin(), djb.end());

    std::printf("    buckets=%zu  FNV-1a longest=%zu  djb2 longest=%d  "
                "s[0]+s.size() longest=%d (only %zu buckets used)\n",
                buckets, r.max_probe_length(), djb_longest, weak_longest, weak_used);

    CHECK(static_cast<std::size_t>(weak_longest) > 100 * r.max_probe_length());
}

TEST(reg_pointer_stable_across_resizes) {
    ToolRegistry r;
    std::vector<Tool*> ptrs;
    for (int i = 0; i < 500; ++i) {
        auto t = std::make_unique<NamedTool>(tool_name(i));
        ptrs.push_back(t.get());
        REQUIRE(r.add(std::move(t)));
    }
    for (int i = 0; i < 500; ++i) CHECK(r.find(tool_name(i)) == ptrs[static_cast<std::size_t>(i)]);
    // The tools still work through the old pointers.
    TempDir d;
    ToolContext ctx{Workspace(d.path())};
    ToolResult res = ptrs[0]->invoke(JsonValue::object(), ctx);
    CHECK(res.ok);
    CHECK(res.content == "tool_0");
}

// ------------------------------------------------------------- workspace

namespace {

void expect_accept(const Workspace& ws, const std::string& in, const fs::path& want) {
    fs::path out;
    std::string reason;
    bool ok = ws.resolve(in, &out, &reason);
    CHECK(ok);
    if (ok) CHECK(out == want);
    if (!ok || out != want)
        std::printf("    resolve(\"%s\") -> ok=%d out=%s reason=%s\n", in.c_str(), ok,
                    out.string().c_str(), reason.c_str());
}

void expect_reject(const Workspace& ws, const std::string& in) {
    fs::path out;
    std::string reason;
    bool ok = ws.resolve(in, &out, &reason);
    CHECK(!ok);
    CHECK(!reason.empty());
    if (ok) std::printf("    resolve(\"%s\") wrongly accepted -> %s\n", in.c_str(),
                        out.string().c_str());
}

}  // namespace

TEST(ws_accepts_table_rows) {
    TempDir d;
    Workspace ws(d.path());
    const fs::path& root = ws.root();
    expect_accept(ws, "notes/todo.txt", root / "notes" / "todo.txt");
    expect_accept(ws, "./notes//todo.txt", root / "notes" / "todo.txt");
    expect_accept(ws, ".", root);
    expect_accept(ws, "a/../b", root / "b");
    expect_accept(ws, "a/..", root);
    expect_accept(ws, "new/dir/file.txt", root / "new" / "dir" / "file.txt");
    expect_accept(ws, "my..notes.txt", root / "my..notes.txt");
}

TEST(ws_rejects_table_rows) {
    TempDir d;
    Workspace ws(d.path());
    expect_reject(ws, "");
    expect_reject(ws, std::string("notes") + '\0' + "/x");
    expect_reject(ws, "/etc/passwd");
    expect_reject(ws, "..");
    expect_reject(ws, "../etc/passwd");
    expect_reject(ws, "a/../../b");
}

TEST(ws_rejects_escape_that_comes_back) {
    TempDir d;
    fs::create_directories(d.path() / "ws" / "notes");
    write_text(d.path() / "ws" / "notes" / "todo.txt", "x");
    Workspace ws(d.path() / "ws");
    expect_reject(ws, "notes/../../ws/notes/todo.txt");
    expect_reject(ws, "../ws/notes/todo.txt");
}

TEST(ws_empty_reason_text) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    CHECK(!ws.resolve("", &out, &reason));
    CHECK(reason == "empty path");
}

TEST(ws_does_not_touch_filesystem) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("new/dir/file.txt", &out, &reason));
    CHECK(!fs::exists(ws.root() / "new"));
    CHECK(!fs::exists(out));
}

// ------------------------------------------------------------- calculator

namespace {

void expect_value(const std::string& expr, double want) {
    double v = 0;
    std::string err;
    bool ok = eval(expr, &v, &err);
    CHECK(ok);
    CHECK(v == want);
    if (!ok || v != want)
        std::printf("    eval(\"%s\") -> ok=%d value=%g error=%s (want %g)\n", expr.c_str(),
                    ok, v, err.c_str(), want);
}

}  // namespace

TEST(calc_table_rows) {
    expect_value("2 + 3 * 4", 14);
    expect_value("10 - 4 - 3", 3);
    expect_value("100 / 10 / 5", 2);
    expect_value("(2 + 3) * 4", 20);
    expect_value("2 * (3 + 4) - 5", 9);
    expect_value("-3 * -3", 9);
    expect_value("7 / 2", 3.5);
    double v = 0;
    std::string err;
    CHECK(!eval("1 / (2 - 2)", &v, &err));
    CHECK(err == "division by zero");
}

TEST(calc_left_to_right) {
    expect_value("10 - 4 - 3", 3);       // not 10 - (4 - 3) = 9
    expect_value("2 * 6 / 3", 4);
    expect_value("12 / 2 * 3", 18);      // not 12 / 6 = 2
    expect_value("1 - 2 + 3", 2);
}

TEST(calc_parentheses_and_nesting) {
    expect_value("((((7))))", 7);
    expect_value("(1 + 2) * (3 + 4)", 21);
    expect_value("2 * (3 + (4 - 1) * 2)", 18);
    expect_value("(12 + 30) * 2 / 7", 12);
}

TEST(calc_negatives_and_decimals) {
    expect_value("-5", -5);
    expect_value("2 * -3", -6);
    expect_value("(-1) - -1", 0);
    expect_value("0.5 + 0.25", 0.75);
    expect_value("3.5 * 2", 7);
    expect_value("-2.5 / 0.5", -5);
}

TEST(calc_division_by_zero_deep) {
    double v = 0;
    std::string err;
    CHECK(!eval("5 + (3 * 2) / (4 - 4) + 1", &v, &err));
    CHECK(err == "division by zero");
    CHECK(!eval("0 / 0", &v, &err));
    CHECK(err == "division by zero");
}

TEST(calc_very_long_expressions) {
    std::string sum = "1";
    for (int i = 1; i < 20000; ++i) sum += "+1";
    expect_value(sum, 20000);

    std::string nested;
    for (int i = 0; i < 5000; ++i) nested += "(";
    nested += "42";
    for (int i = 0; i < 5000; ++i) nested += ")";
    expect_value(nested, 42);
}

TEST(calc_malformed_tokens_do_not_crash) {
    using K = CalcToken::Kind;
    double v = 0;
    std::string err;
    auto num = [](double x) { CalcToken t; t.kind = K::Number; t.value = x; return t; };
    auto op = [](K k) { CalcToken t; t.kind = k; return t; };

    CHECK(!calc_evaluate({}, &v, &err));
    CHECK(!err.empty());
    err.clear();
    CHECK(!calc_evaluate({op(K::Plus)}, &v, &err));
    CHECK(!err.empty());
    err.clear();
    CHECK(!calc_evaluate({num(1), op(K::RParen)}, &v, &err));
    CHECK(!err.empty());
    err.clear();
    CHECK(!calc_evaluate({op(K::LParen), num(1)}, &v, &err));
    CHECK(!err.empty());
    err.clear();
    CHECK(!calc_evaluate({num(1), num(2)}, &v, &err));
    CHECK(!err.empty());
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
