# Simple Hash-based Search (with tiny LRU)

This is a tiny, single-file C program that scans a few `.txt` files in the `texts/` folder, builds a word → files index using a hash table, and answers user queries. A 5-entry LRU cache speeds up repeated lookups. The cache is implemented as a tiny fixed array (no pointers or linked lists).

- Run flow: index once → prompt → you type a word → it prints which files contain that word.
- Scope: alphanumeric words only; all matching is case-insensitive.

---

## Data structures (what lives in memory)

- `FileNode` (linked list): keeps filenames where a word appears.
  - `name`: the filename (e.g., `doc1.txt`)
  - `next`: next node

- `WordEntry` (hash table bucket node): one per unique word
  - `word`: the word, already lowercase
  - `files`: head of a linked list of `FileNode` entries
  - `next`: next word in the same bucket (separate chaining)

- `CacheSlot` (fixed array, capacity 5): LRU cache slot for a word
  - `word`: pointer to the same word string kept in the hash table
  - `files`: pointer to the same `FileNode` list stored in the hash table (no copying)
  - We keep up to 5 slots and move the hit to the front by shifting entries.

- Hash table globals
  - `#define HASH_SIZE 10007` and `static WordEntry *hash_table[HASH_SIZE];`
  - `hash_table[i]` points to a chain of `WordEntry` nodes.

- LRU globals
  - `cache_slots[CACHE_CAP]` stores up to 5 recent hits; index 0 is MRU
  - `cache_count` tracks how many slots are filled

---

## Functions explained, step by step

### `hash_str(const char *s)`
- Purpose: turn a word into a bucket index.
- How: uses the classic djb2 hash. Starts at 5381 and for each character `c` does `h = h * 33 + c`. Finally returns `h % HASH_SIZE`.
- Why: simple, fast, decent distribution for small projects.

### `filelist_contains(FileNode *head, const char *filename)`
- Walks the `FileNode` linked list and checks `strcmp(p->name, filename) == 0`.
- Returns 1 if found, else 0.
- Used to avoid adding the same filename twice for a word.

### `add_file_to_list(WordEntry *e, const char *filename)`
- Guards: if `e` is null, return. If the filename is already present, return.
- Allocates a new `FileNode`, then allocates and copies the filename string.
- Pushes the node at the front: `node->next = e->files; e->files = node;`
- Effect: `e->files` now includes this filename.

### `find_word_entry(const char *word)`
- Computes the bucket index: `idx = hash_str(word)`.
- Walks the chain at `hash_table[idx]` and returns the node where `strcmp(p->word, word) == 0`.
- Returns NULL if not present.

### `insert_word(const char *word, const char *filename)`
- If word is empty, bail.
- Look up the bucket (`hash_str`) and traverse the chain.
  - If found, just call `add_file_to_list` and return.
  - If not found, allocate a new `WordEntry`, allocate/copy the `word`, and insert at the front of that bucket chain.
- Then add the filename to this new entry’s file list.

### `index_file(const char *fullpath, const char *displayname)`
- Opens the file for reading. If it can’t open, returns silently.
- Reads char-by-char.
  - If `isalnum(ch)`, lowercases and appends to a small buffer `buf` (up to 255 chars).
  - If `!isalnum`, the current buffer is a finished word: terminate with `\0` and call `insert_word(buf, displayname)`, then reset length.
- At EOF, finish any partial word.
- Closes the file.
- Result: every alphanumeric word in the file is indexed under `displayname`.

### `sample_full_paths` and `sample_basenames` (constant arrays)
- Two small hardcoded lists:
  - full paths: `"texts/doc1.txt"`, `"texts/doc2.txt"`, `"texts/doc3.txt"`
  - base names: `"doc1.txt"`, `"doc2.txt"`, `"doc3.txt"`
- We pass the full path to open the file and the base name for display; simpler than parsing paths.

Removed — we avoid path parsing by using separate arrays for base names and full paths.

### `index_static_files(void)`
- For each index `i`, call `index_file(sample_full_paths[i], sample_basenames[i])`.
- This builds the whole index once at program start.

### LRU cache helpers (array-based)

#### `cache_index_of(const char *word)`
- Linear search over the at-most-5 slots; returns index or -1.

#### `cache_get(const char *word)`
- If found at index `i`, move that slot to the front (index 0) by shifting entries, then return its `files`.
- If not found, return NULL.

#### `cache_put(const char *word_ptr_in_table, FileNode *files)`
- If the word is already present, update `files` and move to front.
- If not present, insert at the front; if full, evict the last slot (index 4) by shifting.
- We store the `word` pointer from the hash table, so we don’t duplicate strings.

### `lookup_with_cache(const char *word)`
- First checks the cache via `cache_get(word)`.
  - If hit, returns `files`.
- If miss, look up `find_word_entry(word)`.
  - If found, call `cache_put(e->word, e->files)` (store pointer to the table’s word) and return `e->files`.
  - Else return NULL.

### UI helpers

#### `normalize_word_inplace(char *s)`
- Goes through the input string and keeps only alphanumeric chars, lowercased.
- Compacts in-place so we don’t need extra buffers.
- This makes searches case-insensitive and ignores punctuation.

#### `print_files(FileNode *files)`
- If list is empty, prints `not found`.
- Otherwise prints `found in: ` then every filename on one line, space-separated.

### `main(int argc, char **argv)`
- Ignores command-line args to keep it simple.
- Calls `index_static_files()` once to build the index.
- Prints a tiny prompt and loops:
  1. Read a line with `fgets`.
  2. Strip `\n`/`\r` at the end.
  3. If empty, exit.
  4. `normalize_word_inplace(line)`.
  5. Call `lookup_with_cache(line)`.
  6. If found, `print_files(files)`, else print `not found`.
- Exits without freeing everything; the OS reclaims memory on exit (fine for a small class assignment).

---

## How the pieces fit (big picture)

1) Indexing time:
- For each file, parse words and call `insert_word(word, filename)`.
- `insert_word` puts the word in the hash table and makes sure the filename appears once.

2) Query time:
- You type a word.
- We normalize it (lowercase, drop punctuation).
- Check the LRU cache first. If cached, return fast.
- Otherwise, look in the hash table and, if found, store it in the cache for next time.

---

## Complexity (rough idea)

- Indexing:
  - Let T = total number of words across files (after normalization).
  - Average-case `insert_word` is O(1) due to hashing; overall about O(T).
- Query:
  - Cache: O(C) with C = 5 (find + small shifts). That’s effectively constant.
  - Hash table lookup average-case O(1). Worst-case with bad distribution becomes O(N) in the bucket.

---

## Common questions you might get

- Why hashing? 
  - Fast average O(1) lookup for words → files.

- Why linked lists for collisions?
  - Simple separate chaining in the hash table. For a small project, it’s fine and easy to code.

- What about punctuation and case?
  - We only keep letters and digits and lowercase everything, so “Data,” “data,” and “DATA” all become `data`.

- Does the cache store negatives (misses)?
  - No. Only positive hits are cached. Simpler logic, less memory.

- Why not free memory at the end?
  - Program is tiny and exits; the OS will reclaim memory. For longer-running apps, you’d add cleanup.

- Where do the filenames come from?
  - We hardcode `texts/doc1.txt`, `texts/doc2.txt`, `texts/doc3.txt` to keep it simple for class.

---

## Tweaks you can do quickly

- Change cache capacity: edit `#define CACHE_CAP 5`.
- Add more files: append paths to `sample_files`.
- Make it case-sensitive: remove the `tolower` calls in both parsing and normalization.
- Include hyphenated words: change the `isalnum` checks to allow `-` (and adjust parsing rules accordingly).

---

## Try it

```bash
make main
./main
# try: data
# try: hash
# try: cache
```
