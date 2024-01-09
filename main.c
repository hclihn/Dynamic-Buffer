#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#define MIN(a, b)           (((a) < (b))?(a):(b))
#define DYN_BUF_SZ          (16)
#define DYN_BUF_INIT_SZ     (2)

typedef struct {
    char** array;
    size_t curIdx;
    size_t curSize;
    size_t arraySize;
} DynamicBuffer;

void showError(char *fmt,...) {
    va_list args;

    fprintf(stderr, "ERROR: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (fmt[strlen(fmt)-1] != '\n') {
        fprintf(stderr, "\n");
    }
}

int _allocate_DynamicBuffer(DynamicBuffer* pDb) {
    if (pDb->array[pDb->curIdx] != NULL)    return 0;
    printf("allocate array[%ld] buffer\n", pDb->curIdx);
    pDb->array[pDb->curIdx] = malloc(DYN_BUF_SZ);
    if (pDb->array[pDb->curIdx] == NULL) {
        showError("Failed to allocate buffer #%d (%d bytes) of DynamicBuffer during init!",
            pDb->curIdx, DYN_BUF_SZ);
        return 1;
    }
    return 0;
}

int init_DynamicBuffer(DynamicBuffer* pDb) {
    pDb->array = (char**) malloc(DYN_BUF_INIT_SZ * sizeof(char*));
    if (pDb->array == NULL) {
        showError("Failed to allocate array part (%d bytes) of DynamicBuffer during init!",
            DYN_BUF_INIT_SZ * sizeof(char*));
        return 1;
    }
    pDb->arraySize = DYN_BUF_INIT_SZ;
    for (size_t i = 0; i < pDb->arraySize; i++) {
        pDb->array[i] = NULL;
    }
    pDb->curIdx = 0;
    int rc = _allocate_DynamicBuffer(pDb);
    if (rc != 0)    return rc;

    pDb->curSize = 0;
    return 0;
}

int add_DynamicBuffer(DynamicBuffer* pDb, char *b, size_t s) {
    if (pDb->array == NULL || pDb->arraySize == 0) {
        showError("Uninitialized DynamicBuffer specified!");
        return 1;
    }
    while (s > 0) {
        size_t left = DYN_BUF_SZ - pDb->curSize;
        if (left == 0) {
            if (pDb->curIdx == pDb->arraySize - 1) { // full
                size_t ns = pDb->arraySize * 2;
                char** np = realloc(pDb->array, ns * sizeof(char*));
                if (np == NULL) {
                    showError("Failed to reallocate array part (%d bytes) of DynamicBuffer during add!",
                        ns * sizeof(char*));
                    return 1;
                }
                pDb->array = np;
                for (size_t i = pDb->arraySize; i < ns; i++) {
                    pDb->array[i] = NULL;
                }
                pDb->arraySize = ns;
            }
            pDb->curIdx++;
            int rc = _allocate_DynamicBuffer(pDb);
            if (rc != 0)    return rc;
            pDb->curSize = 0;
            left = DYN_BUF_SZ - pDb->curSize;
        }
        size_t sz = MIN(s, left);
        memcpy(pDb->array[pDb->curIdx] + pDb->curSize, b, sz);
        pDb->curSize += sz;
        b += sz;
        s -= sz;
    }
    return 0;
}

void free_DynamicBuffer(DynamicBuffer* pDb) {
    if (pDb->array != NULL) {
        for (size_t i = 0; i < pDb->arraySize; i++) {
            if (pDb->array[i] != NULL) {
                free(pDb->array[i]);
            }
        }
        free(pDb->array);
        pDb->array = NULL;
    }
    pDb->arraySize = 0;
    pDb->curIdx = 0;
    pDb->curSize = 0;
}

int toFile_DynamicBuffer(const DynamicBuffer* pDb, FILE *fp) {
    if (pDb->array == NULL || pDb->arraySize == 0) {
        return 0;
    }
    int r = 0;
    for (size_t i = 0; i <= pDb->curIdx; i++) {
        size_t s = (i != pDb->curIdx)? DYN_BUF_SZ : pDb->curSize;
        if (s == 0)     break; // done
        int rc = fwrite(pDb->array[i], 1, s, fp);
        if (rc != s) {
            showError("Failed to write dynamic buffer (@array #%d) to file: %s!", i, strerror(errno));
            return -1;
        }
        r += rc;
    }
    return r;
}

int printf_DynamicBuffer(DynamicBuffer* pDb, const char* fmt, ...) {
    int size = 16, rc = 0, n;
    char *p = NULL;
    va_list ap;

    if ((p = malloc(size)) == NULL) {
        showError("Failed to allocate buffer for printf_DynamicBuffer!");
        rc = -1;
        goto clean_up;
    }

    while(1) {
        va_start(ap, fmt);
        n = vsnprintf(p, size, fmt, ap);
        va_end(ap);

        if (n < 0) {
            showError("Failed to format string for printf_DynamicBuffer!");
            rc = n;
            goto clean_up;
        }

        if (n < size) { // done
            break;
        }

        size = n + 1;
        char* np = realloc(p, size);
        if (np == NULL) {
            showError("Failed to reallocate buffer for printf_DynamicBuffer!");
            rc = -1;
            goto clean_up;
        }
        p = np;
    }
    rc = add_DynamicBuffer(pDb, p, n);
    if (rc != 0) {
        showError("Failed to add formated string to DynamicBuffer!");
        goto clean_up;
    }   

clean_up:
    if (p != NULL)  free(p);
    return rc;
}

size_t len_DynamicBuffer(DynamicBuffer* pDb) {
    if (pDb->array == NULL || pDb->arraySize == 0) {
        return 0;
    }
    size_t l = 0;
    for (size_t i = 0; i <= pDb->curIdx; i++) {
        l += (i != pDb->curIdx)? DYN_BUF_SZ : pDb->curSize;
    }
    return l;
}

int addStr_DynamicBuffer(DynamicBuffer* pDb, char *b) {
    return add_DynamicBuffer(pDb, b, strlen(b));
}

int clear_DynamicBuffer(DynamicBuffer* pDb) {
    if (pDb->array == NULL || pDb->arraySize == 0) {
        return init_DynamicBuffer(pDb);
    }
    pDb->curIdx = 0;
    pDb->curSize = 0;
    return 0;
}

void debug_DynamicBuffer(const DynamicBuffer* pDb) {
  printf("DynamicBuffer{size=%ld, curIdx=%ld, curSz=%ld,\n", pDb->arraySize, pDb->curIdx, pDb->curSize);
  for (size_t i = 0; i <= pDb->curIdx; i++) {
    int s = (i != pDb->curIdx)? DYN_BUF_SZ : pDb->curSize;
    if (s > 0) {
      printf("Buf[%ld](len=%d): '%*s',\n", i, s, s, pDb->array[i]);
    } else {
      printf("Buf[%ld](len=%d),\n", i, s);
    }
  }
  printf("}\n");
}

int main(void) {
  char* ins[] = {
    "12345678901234567890 ",
    "Test this one if you can! ",
    "What about this one? ",
    "Did you see a difference?..."
  };
  DynamicBuffer db;
  init_DynamicBuffer(&db);
  printf("Hello World %ld, %ld\n", sizeof(ins)/sizeof(char*), len_DynamicBuffer(&db));

  size_t ss = 0;
  for (int i = 0; i < sizeof(ins)/sizeof(char*); i++) {
    addStr_DynamicBuffer(&db, ins[i]);
    ss += strlen(ins[i]);
  }
  debug_DynamicBuffer(&db);
  size_t sp = toFile_DynamicBuffer(&db, stdout);
  printf("\nDone! %ld, %ld == %ld\n", ss, sp, len_DynamicBuffer(&db));

  clear_DynamicBuffer(&db);
  debug_DynamicBuffer(&db);

  ss = 0;
  for (int i = sizeof(ins)/sizeof(char*)-1; i >=0; i--) {
    addStr_DynamicBuffer(&db, ins[i]);
    ss += strlen(ins[i]);
  }
  printf_DynamicBuffer(&db, "Let's test this one: %d! ", 102);
  debug_DynamicBuffer(&db);
  sp = toFile_DynamicBuffer(&db, stdout);
  printf("\nDone! %ld, %ld == %ld\n", ss, sp, len_DynamicBuffer(&db));

  free_DynamicBuffer(&db);
  return 0;
}