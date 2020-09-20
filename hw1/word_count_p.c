/*
 * Implementation of the word_count interface using Pintos lists and pthreads.
 *
 * You may modify this file, and are expected to modify it.
 */

/*
 * Copyright © 2019 University of California, Berkeley
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PINTOS_LIST
#error "PINTOS_LIST must be #define'd when compiling word_count_lp.c"
#endif

#ifndef PTHREADS
#error "PTHREADS must be #define'd when compiling word_count_lp.c"
#endif

#include "word_count.h"

void init_words(word_count_list_t *wclist) {
  list_init(&(wclist -> lst));
  pthread_mutex_init(&(wclist -> lock), NULL);
}

size_t len_words(word_count_list_t *wclist) {
  return list_size(&(wclist -> lst));
}

word_count_t *find_word(word_count_list_t *wclist, char *word) {
  struct list_elem *e;

  bool is_empty = list_empty(&(wclist -> lst));
  if (is_empty) {
    return NULL;
  }

  for (e = list_begin (&(wclist -> lst)); e != list_end (&(wclist -> lst)); e = list_next (e)) {
    word_count_t *f = list_entry (e, word_count_t, elem);
    if (f->word) {
      if (strcmp(f -> word, word) == 0) {
	return f;
      }
    }
  }
}

word_count_t *add_word(word_count_list_t *wclist, char *word) {
  word_count_t *wct = find_word(wclist, word);
  if (wct) {
    wct -> count++;
    return wct;
  } else {
    wct = malloc(sizeof(word_count_t));
    wct->word = malloc(sizeof(word));
    wct->word = word;
    wct->count = 1;
    pthread_mutex_lock(&(wclist -> lock));
    list_push_back(&(wclist->lst), &(wct->elem));
    pthread_mutex_unlock(&(wclist -> lock));
    return wct;
  }
}

void fprint_words(word_count_list_t *wclist, FILE *outfile) {
  struct list_elem *e;
  for (e = list_begin (&(wclist->lst)); e != list_end (&(wclist->lst)); e = list_next (e)) {
    word_count_t *f = list_entry (e, word_count_t, elem);
    fprintf(outfile, "%8d\t%s\n", f->count, f->word);
  }
}

static bool less_list(const struct list_elem *ewc1,
                      const struct list_elem *ewc2, void *aux) {
  word_count_t *wc1 = list_entry (ewc1, word_count_t, elem);
  word_count_t *wc2 = list_entry (ewc2, word_count_t, elem); 
  int compare = wc1->count - wc2 -> count;
  if (compare == 0) {
    return strcmp(wc1->word, wc2->word) >=0 ? false : true;
  }
  return compare >= 0 ? false : true;
  return NULL;
}

void wordcount_sort(word_count_list_t *wclist,
                    bool less(const word_count_t *, const word_count_t *)) {
  list_sort(&(wclist->lst), less_list, less);
}
