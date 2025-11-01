# Case Study for 23EAC203

This mini-project is a demonstration of how files can be indexed into hash tables, and searched with a constant average time complexity.
LRU caching is used to speed up the searching even more by keeping the last n hot words at the front in a cache.

**On every search**:
LRU cache is checked. If not in cache, hash table is looked up and result is saved in cache.

### Why LRU Caching?
To save time in a larger dataset. If the search query is in the cache, program avoids the following entirely:
- Recomputing the hash (no djb2 loop over all characters)
- Following bucket chains, Linked list traversal takes O(n) time
- strcmp calls against each `WordEntry` in a chain
- Consequentially, “find in hash then insert into cache” step is skipped

## Sample search queries
```
indexed files from ./texts (doc1.txt, doc2.txt, doc3.txt)
type a word to search (empty line to quit)
> test 
from hash table
found in: doc1.txt
> small
from hash table
found in: doc3.txt doc1.txt
> test
from cache
found in: doc1.txt
> invalid
not found
```

## How to run
```
$ make cache-search
$ ./cache-search
```
