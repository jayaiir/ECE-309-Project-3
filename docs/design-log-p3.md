# Project 3 Design Log
**Author:** John Hair· **P2 code used:** my own (`conversation.h`, `sentinel_scanner.h` copied from Project 2; `test_p2` passes)
**Tools used:** Claude to review code quality and error checking.

## 1. Hash table

`ToolRegistry` stores `buckets_` (a `std::vector` of buckets) and `size_` (how many tools are registered). Each bucket is a `std::vector<std::unique_ptr<Tool>>`, so I went with **separate chaining**.

I picked chaining over linear probing mostly because it seemed easier to get right. Linear probing needs wrap-around, and its load factor has to stay below 1 or `find` on a missing name loops forever. With chaining, a collision just adds one more entry to a bucket. The spec's table also shows probing with longer worst cases (about 30 vs. 6) because filled slots bunch up.

For the hash I used **FNV-1a (64-bit)** from Appendix A. It XORs in each character and then multiplies, so every character matters and `"ab"` ≠ `"ba"`. `hash()` returns the full 64 bits, and `index_for()` uses `hash & (bucket_count - 1)`, which works because the bucket count is always a power of two (starting at 8).

**Resizing.** My threshold is a load factor of 1.0. Before inserting, `add()` checks whether `size_ + 1` would go over `bucket_count()`. If so, it doubles the buckets first and re-inserts every tool with the new count, since every index changes. Checking *before* inserting keeps `load_factor()` at or below 1.0. I chose 1.0 because with chaining that's still about one tool per bucket, without lots of empty buckets.

**Measurements.** `reg_10000_tools` and `reg_alternative_hash` insert `tool_0` … `tool_9999` and print these:

| Hash | bucket_count | load_factor | longest chain |
|---|---|---|---|
| FNV-1a (my table) | 16,384 | 0.6104 | 6 |
| djb2 (same buckets) | 16,384 | — | 4 |
| `s[0] + s.size()` (same buckets) | 16,384 | — | 9,000 (only 4 buckets used) |

The table resized 11 times (8 -> 16,384). The weak hash was much worse than I expected. Every name starts with `t` and is 6 to 9 characters long, so there are only 4 possible values and `find` turns into a linear search. djb2 did fine with chaining, even though the spec says it clusters badly with linear probing.

**Why `add()` is amortized O(1).** Same idea as `Conversation::append` in P2. A resize from n to 2n re-inserts n tools, but only after about n/2 cheap inserts. All the resizes add up to 8 + 16 + … + N < 2N, so each insert is constant on average.

## 2. Pointer stability

The buckets hold `std::unique_ptr<Tool>`, not the `Tool` objects. Each tool is created once on the heap and stays at that address. A rehash `std::move`s each `unique_ptr` into the new array, which only copies the address, so a `Tool*` from `find()` still points to the right object. The vectors get reallocated, but they only hold pointers.

`reg_10000_tools` checks that the pointer from the first insert is the same after all 11 resizes. `reg_ownership_destroys_once` uses a tool that counts its destructor calls, to check that each tool is destroyed exactly once and a rehash destroys nothing.

## 3. Calculator trace: `2 * (3 + 4) - 5`

1. `2`: push it. values = [2]
2. `*`: ops is empty, so just push it. ops = [*]
3. `(`: always push. ops = [*, (]
4. `3`: values = [2, 3]
5. `+`: the top of ops is `(`, which isn't an operator, so nothing gets applied. Push `+`. ops = [*, (, +]
6. `4`: values = [2, 3, 4]
7. `)`: apply operators until the top is `(`. Pop `+`, then pop 4 (right) and 3 (left), and push 7. values = [2, 7]. Now the top is `(`, so pop it. ops = [*]
8. `-`: the top is `*`, and precedence 2 ≥ 1, so apply it: pop 7 (right) and 2 (left), and push 14. ops is empty now, so push `-`. values = [14], ops = [-]
9. `5`: values = [14, 5]
10. End: apply `-`: pop 5 (right) and 14 (left), and push 9. The answer is 9.

Step 8 is where `>=` matters. With just `>`, `10 - 4 - 3` would give 9 instead of 3.

## 4. Errors as values

In this program, a tool failing is normal. The model might ask for a missing file, try `../etc/passwd`, or divide by zero, and it should be told so it can try something else. Returning `ToolResult::failure(...)` (or `false` plus a message) makes failing just another return value, and the message goes right into the `"ok":false` JSON the model reads. If these threw instead, one uncaught exception would crash the whole harness over something like a typo in a file name. Exceptions make more sense for real bugs in the C++ code, like calling `as_string()` on a number, since those should never happen and should be noticed right away.

## 5. What I would do differently

Every name comparison in `find()` and every re-insert in `rehash()` calls the virtual `name()`, which builds a new `std::string`. With 10,000 tools that adds up. Next time I'd store the name (and maybe the hash) next to the `unique_ptr` in each bucket. Tools never get renamed, so the copy would never be out of date.
