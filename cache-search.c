#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct FileNode {
    char *name;              
    struct FileNode *next;
} FileNode;

// bucket
typedef struct WordEntry {
    char *word; 
    FileNode *files; // linked list of files containing this word
    struct WordEntry *next;
} WordEntry;

// lru cache array
#define CACHE_CAP 5
typedef struct CacheSlot {
    const char *word;   // points to word stored in hash table
    FileNode *files;    // points to list in hash table
} CacheSlot;

static CacheSlot cache_slots[CACHE_CAP];
static int cache_count = 0; // slots in use

//hash table 
#define HASH_SIZE 10007  // small prime
static WordEntry *hash_table[HASH_SIZE];


unsigned long hash_str(const char *s) {
    // djb2 hashing
    unsigned long h = 5381;
    int c;
    while ((c = (unsigned char)*s++)) {
        h = ((h << 5) + h) + (unsigned long)c;
    }
    return h % HASH_SIZE;
}

int filelist_contains(FileNode *head, const char *filename) {
    for (FileNode *p = head; p; p = p->next) {
        if (strcmp(p->name, filename) == 0) return 1;
    }
    return 0;
}

void add_file_to_list(WordEntry *e, const char *filename) {
    if (!e) return;
    if (filelist_contains(e->files, filename)) return; // already there, skip
    FileNode *node = (FileNode *)malloc(sizeof(FileNode));
    if (!node) return;
    node->name = (char *)malloc(strlen(filename) + 1);
    if (!node->name) { free(node); return; }
    strcpy(node->name, filename);
    node->next = e->files;
    e->files = node;
}

WordEntry *find_word_entry(const char *word) {
    unsigned long idx = hash_str(word);
    WordEntry *p = hash_table[idx];
    while (p) {
        if (strcmp(p->word, word) == 0) return p;
        p = p->next;
    }
    return NULL;
}

void insert_word(const char *word, const char *filename) {
    if (!word || !*word) return;
    unsigned long idx = hash_str(word);
    WordEntry *p = hash_table[idx];
    while (p) {
        if (strcmp(p->word, word) == 0) {
            add_file_to_list(p, filename);
            return;
        }
        p = p->next;
    }
    // not found, make new entry
    WordEntry *e = (WordEntry *)malloc(sizeof(WordEntry));
    if (!e) return;
    e->word = (char *)malloc(strlen(word) + 1);
    if (!e->word) { free(e); return; }
    strcpy(e->word, word);
    e->files = NULL;
    e->next = hash_table[idx];
    hash_table[idx] = e;
    add_file_to_list(e, filename);
}

// file pasring
void index_file(const char *fullpath, const char *displayname) {
    FILE *f = fopen(fullpath, "r");
    if (!f) return;

    // take words and push to tabel after lowercase
    char buf[256];
    int len = 0;
    int ch;

    while ((ch = fgetc(f)) != EOF) {
        if (isalnum((unsigned char)ch)) {
            if (len < (int)sizeof(buf) - 1) {
                buf[len++] = (char)tolower((unsigned char)ch);
            }
        } else {
            if (len > 0) {
                buf[len] = '\0';
                insert_word(buf, displayname);
                len = 0;
            }
        }
    }
    if (len > 0) {
        buf[len] = '\0';
        insert_word(buf, displayname);
    }

    fclose(f);
}

// file list indexing
static const char *sample_full_paths[] = {
    "texts/doc1.txt",
    "texts/doc2.txt",
    "texts/doc3.txt",
};

static const char *sample_basenames[] = {
    "doc1.txt",
    "doc2.txt",
    "doc3.txt",
};

void index_static_files(void) {
    int n = (int)(sizeof(sample_full_paths) / sizeof(sample_full_paths[0]));
    for (int i = 0; i < n; ++i) {
        index_file(sample_full_paths[i], sample_basenames[i]);
    }
}

// lru
int cache_index_of(const char *word) {
    for (int i = 0; i < cache_count; ++i) {
        if (strcmp(cache_slots[i].word, word) == 0) return i;
    }
    return -1;
}

FileNode *cache_get(const char *word) {
    int idx = cache_index_of(word);
    if (idx < 0) return NULL;
    // move to front
    CacheSlot hit = cache_slots[idx];
    for (int j = idx; j > 0; --j) cache_slots[j] = cache_slots[j - 1];
    cache_slots[0] = hit;
    return cache_slots[0].files;
}

void cache_put(const char *word_ptr_in_table, FileNode *files) {
    if (!word_ptr_in_table || !files) return;
    int idx = cache_index_of(word_ptr_in_table);
    if (idx >= 0) {
        // update and move to front
        cache_slots[idx].files = files;
        CacheSlot hit = cache_slots[idx];
        for (int j = idx; j > 0; --j) cache_slots[j] = cache_slots[j - 1];
        cache_slots[0] = hit;
        return;
    }
    // insert at front, remove from end
    int limit = (cache_count < CACHE_CAP) ? cache_count : (CACHE_CAP - 1);
    for (int j = limit; j > 0; --j) cache_slots[j] = cache_slots[j - 1];
    cache_slots[0].word = word_ptr_in_table; // hash table pointer
    cache_slots[0].files = files;
    if (cache_count < CACHE_CAP) cache_count++;
}

FileNode *lookup_with_cache(const char *word) {
    FileNode *files = cache_get(word);
    if (files) {
        printf("from cache\n");
        return files; // hot path
    }

    WordEntry *e = find_word_entry(word);
    if (e && e->files) {
        // store pointer to tables word to avoid extra alloc
        cache_put(e->word, e->files);
        printf("from hash table\n");
        return e->files;
    }
    return NULL;
}

// ui helpers

void normalize_word_inplace(char *s) {
    int w = 0;
    for (int r = 0; s[r]; ++r) {
        unsigned char c = (unsigned char)s[r];
        if (isalnum(c)) {
            s[w++] = (char)tolower(c);
        }
    }
    s[w] = '\0';
}

void print_files(FileNode *files) {
    // print filenames on one line, space separated
    int count = 0;
    for (FileNode *p = files; p; p = p->next) {
        if (count == 0) printf("found in: ");
        printf("%s%s", p->name, p->next ? " " : "\n");
        count++;
    }
    if (count == 0) printf("not found\n");
}

int main(int argc, char **argv) {
    // ignore args
    (void)argc; (void)argv;
    index_static_files();
    
    printf("indexed files from ./texts (doc1.txt, doc2.txt, doc3.txt)\n");
    printf("type a word to search (empty line to quit)\n");

    char line[256];
    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        // strip newline
        size_t n = strlen(line);
        if (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (n == 0) break; // empty -> exit

        normalize_word_inplace(line);
        if (line[0] == '\0') {
            printf("pls type letters/numbers\n");
            continue;
        }

        FileNode *files = lookup_with_cache(line);
        if (files) {
            print_files(files);
        } else {
            printf("not found\n");
        }
    }
    return 0;
}
