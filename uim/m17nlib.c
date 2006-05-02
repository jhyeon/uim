/*

  Copyright (c) 2003-2006 uim Project http://uim.freedesktop.org/

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of authors nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.

*/

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <m17n.h>
#include "uim-scm.h"
#include "uim-util.h"
#include "plugin.h"

static int m17nlib_ok;
static MConverter *converter;
static char buffer_for_converter[1024]; /* Currently, if preedit strings or
					   candidate strings over this buffer
					   size, they will simply ignore. */

static int nr_input_methods;
static struct im_ {
  char *lang;
  char *name;
  MInputMethod *im;
} *im_array;

static int max_input_contexts;
static struct ic_ {
  MInputContext *mic;
  char **old_candidates; /* FIXME: ugly hack for perfomance... */
  char **new_candidates; /* FIXME: ugly hack for perfomance... */
  int  nr_candidates;
} *ic_array;

static char *
m17nlib_utf8_find_next_char(const char *p);

static int
unused_ic_id(void)
{
  int i;
  for (i = 0; i < max_input_contexts; i++) {
    if (!ic_array[i].mic) {
      return i;
    }
  }
  ic_array = realloc(ic_array, sizeof(struct ic_) * (max_input_contexts+1));
  ic_array[max_input_contexts].mic = NULL;
  max_input_contexts++;
  return max_input_contexts - 1;
}

static void
pushback_input_method(MInputMethod *im,
		      char *lib_lang, char *name)
{
  const char *lang = uim_get_language_code_from_language_name(lib_lang);

  im_array = realloc(im_array, 
		     sizeof(struct im_) * (nr_input_methods + 1));

  if (lang != NULL) {
    im_array[nr_input_methods].lang = strdup(lang);
  } else {
    im_array[nr_input_methods].lang = NULL;
  }
  im_array[nr_input_methods].name = strdup(name);
  im_array[nr_input_methods].im = im;
  nr_input_methods++;
}

#if 0
static void
preedit_start_cb(MInputContext *ic, MSymbol command)
{
  fprintf(stderr,"preedit start\n");
}

static void 
preedit_draw_cb(MInputContext *ic, MSymbol command)
{

}

static void 
preedit_done_cb(MInputContext *ic, MSymbol command)
{
  /*  fprintf(stderr,"preedit done\n");*/
}

static void
status_start_cb(MInputContext *ic, MSymbol command)
{
  /*  fprintf(stderr,"status start\n");*/
}

static void 
status_draw_cb(MInputContext *ic, MSymbol command)
{
  /*  fprintf(stderr,"status draw\n"); */
}

static void 
status_done_cb(MInputContext *ic, MSymbol command)
{
  /*  fprintf(stderr,"status done\n");*/
}

static void
candidates_start_cb(MInputContext *ic, MSymbol command)
{
  /*  fprintf(stderr,"candidates_start\n");*/
}

static void 
candidates_draw_cb(MInputContext *ic, MSymbol command)
{
  /*  fprintf(stderr,"candidate draw\n"); */
}

static void 
candidates_done_cb(MInputContext *ic, MSymbol command)
{
  /*  fprintf(stderr,"candidate done\n"); */
}

static void
register_callbacks(void)
{
  /*
  mplist_add(minput_default_driver.callback_list, Minput_preedit_start, (void *)preedit_start_cb);
  mplist_add(minput_default_driver.callback_list, Minput_preedit_draw,  (void *)preedit_draw_cb);
  mplist_add(minput_default_driver.callback_list, Minput_preedit_done,  (void *)preedit_done_cb);
    mplist_add(minput_default_driver.callback_list, Minput_status_start,  (void *)status_start_cb);
  mplist_add(minput_default_driver.callback_list, Minput_status_draw,   (void *)status_draw_cb);
  mplist_add(minput_default_driver.callback_list, Minput_status_done,   (void *)status_done_cb);
  mplist_add(minput_default_driver.callback_list, Minput_candidates_start, (void *)candidates_start_cb);
  mplist_add(minput_default_driver.callback_list, Minput_candidates_draw,  (void *)candidates_draw_cb);
  mplist_add(minput_default_driver.callback_list, Minput_candidates_done,  (void *)candidates_done_cb);*/
}
#endif
static uim_lisp
init_m17nlib()
{
  MPlist *imlist, *elm;
  MSymbol utf8 = msymbol("utf8");
  M17N_INIT();
  nr_input_methods = 0;
  im_array = NULL;
  max_input_contexts = 0;
  ic_array = NULL;

  imlist = mdatabase_list(msymbol("input-method"), Mnil, Mnil, Mnil);
  if (!imlist) {
    /* maybe user forgot to install m17n-db */
    return uim_scm_f();
  }
  for (elm = imlist; mplist_key(elm) != Mnil; elm = mplist_next(elm)) {
    MDatabase *mdb = mplist_value(elm);
    MSymbol *tag = mdatabase_tag(mdb);
    if (tag[1] != Mnil) {
      MInputMethod *im = minput_open_im(tag[1], tag[2], NULL);
      if (im) {
	MSymbol lang = msymbol_get(im->language, Mlanguage);
	pushback_input_method(im, msymbol_name(lang),
			      msymbol_name(im->name));
	
      }
    }
  }
  /*  register_callbacks();*/
  m17n_object_unref(imlist);
  converter = mconv_buffer_converter(utf8, NULL, 0);
  if (!converter) {
    return uim_scm_f();
  }
  m17nlib_ok = 1;
  return uim_scm_t();
}

static char *
convert_mtext2str(MText *mtext)
{
  mconv_rebind_buffer(converter, (unsigned char *)buffer_for_converter,
		      sizeof(buffer_for_converter));
  mconv_encode(converter, mtext);
  buffer_for_converter[converter->nbytes] = 0;
  return strdup(buffer_for_converter);
}

static uim_lisp
compose_modep(uim_lisp id_)
{
  int id = uim_scm_c_int(id_);
  /* range check of id might need. */
  MInputContext *ic = ic_array[id].mic;
  if (!ic) {
    return uim_scm_f();
  }
  if (ic->candidate_from == ic->candidate_to ||
     ic->candidate_from > ic->candidate_to) {
    return uim_scm_f();
  } else {
    return uim_scm_t();
  }
}

static uim_lisp
preedit_changedp(uim_lisp id_)
{
  int id = uim_scm_c_int(id_);
  /* range check of id might need. */
  MInputContext *ic = ic_array[id].mic;
  if (!ic) {
    return uim_scm_f();
  }
  if (ic->preedit_changed == 1) {
    return uim_scm_t();
  } else {
    return uim_scm_f();
  }
}

static uim_lisp
get_left_of_cursor(uim_lisp id_)
{
  int id = uim_scm_c_int(id_);
  int buflen;
  int i;
  uim_lisp buf_;
  char *buf;
  char *p;
  MInputContext *ic = ic_array[id].mic;
  if (!ic) {
    return uim_scm_make_str("");
  }
  if (ic->cursor_pos == 0) {
    return uim_scm_make_str("");
  }
  buf = convert_mtext2str(ic->preedit);
  p = (char *)buf;

  for (i=0; i<ic->cursor_pos ;i++) {
    p = m17nlib_utf8_find_next_char(p);
  }
  *p = 0;

  buflen = strlen((char *)buf);
  buf_ = uim_scm_make_str((char *)buf);
  return buf_;
}

static uim_lisp
get_right_of_cursor(uim_lisp id_)
{
  int id = uim_scm_c_int(id_);
  int buflen;
  int i;
  uim_lisp buf_;
  char *buf;
  char *p;
  MInputContext *ic = ic_array[id].mic;
  if (!ic) {
    return uim_scm_make_str("");
  }

  buf = convert_mtext2str(ic->preedit);
  p = buf;

  for (i=0; i<ic->cursor_pos ;i++) {
    p = m17nlib_utf8_find_next_char(p);
  }
  buflen = strlen((char *)p);
  buf_ = uim_scm_make_str((char *)p);
  return buf_;
}

static uim_lisp
get_left_of_candidate(uim_lisp id_)
{
  int id = uim_scm_c_int(id_);
  int buflen;
  int i;
  uim_lisp buf_;
  char *buf;
  char *p;
  MInputContext *ic = ic_array[id].mic;
  if (!ic) {
    return uim_scm_make_str("");
  }
  if (ic->candidate_from == 0) {
    return uim_scm_make_str("");
  }
  buf = convert_mtext2str(ic->preedit);
  p = buf;

  for (i=0; i<ic->candidate_from ;i++) {
    p = m17nlib_utf8_find_next_char(p);
  }
  *p = 0;
  buflen = strlen((char *)buf);
  buf_ = uim_scm_make_str((char *)buf);
  free(buf);
  return buf_;
}

static uim_lisp
get_selected_candidate(uim_lisp id_)
{
  int id = uim_scm_c_int(id_);
  int buflen;
  int i;
  uim_lisp buf_;
  char *buf;
  char *p;
  char *start;
  MInputContext *ic = ic_array[id].mic;
  if (!ic) {
    return uim_scm_make_str("");
  }
  buf = convert_mtext2str(ic->preedit);
  p = buf;

  if (!p) {
    return uim_scm_make_str("");
  }

  for (i=0; i<ic->candidate_from ;i++) {
    p = m17nlib_utf8_find_next_char(p);
  }
  start = p;

  for (i=0; i<ic->candidate_to - ic->candidate_from ;i++) {
    p = m17nlib_utf8_find_next_char(p);
  }
  *p = 0;

  buflen = strlen(start);
  buf_ = uim_scm_make_str(start);
  free(buf);
  return buf_;
}

static uim_lisp
get_right_of_candidate(uim_lisp id_)
{
  int id = uim_scm_c_int(id_);
  int buflen;
  int i;
  uim_lisp buf_;
  char *buf;
  char *p;
  MInputContext *ic = ic_array[id].mic;
  if (!ic) {
    return uim_scm_make_str("");
  }
  buf = convert_mtext2str(ic->preedit);
  p = buf;

  for (i=0; i<ic->candidate_to ;i++) {
    p = m17nlib_utf8_find_next_char(p);
  }
  buflen = strlen(p);
  buf_ = uim_scm_make_str(p);
  free(buf);
  return buf_;
}

static uim_lisp
get_nr_input_methods()
{
  return uim_scm_make_int(nr_input_methods);
}

static uim_lisp
get_input_method_name(uim_lisp nth_)
{
  int nth = uim_scm_c_int(nth_);
  if (nth < nr_input_methods) {
    char *name = alloca(strlen(im_array[nth].name) + 20);

    if (im_array[nth].lang != NULL)
      sprintf(name, "m17n-%s-%s", im_array[nth].lang, im_array[nth].name);
    else
      sprintf(name, "m17n-%s", im_array[nth].name);

    return uim_scm_make_str(name);
  }
  return uim_scm_f();
}

static uim_lisp
get_input_method_lang(uim_lisp nth_)
{
  int nth = uim_scm_c_int(nth_);
  if (nth < nr_input_methods) {
    char *lang = im_array[nth].lang;
    if (lang != NULL) {
      return uim_scm_make_str(lang);
    } else {
      return uim_scm_make_str("unknown");
    }
  }
  return uim_scm_f();
}

static MInputMethod *
find_im_by_name(char *name)
{
  int i;
  if (strncmp(name, "m17n-", 5) != 0) {
    return NULL;
  }
  name = &name[5];
  for (i = 0; i < nr_input_methods; i++) {
    char buf[100];
    if (im_array[i].lang == NULL) {
      snprintf(buf, 100, "%s", im_array[i].name);
    } else {
      snprintf(buf, 100, "%s-%s", im_array[i].lang, im_array[i].name);
    }
    if (!strcmp(name, buf)) {
      return im_array[i].im;
    }
  }
  return NULL;
}

static uim_lisp
alloc_id(uim_lisp name_)
{
  int id = unused_ic_id();
  char *name = uim_scm_c_str(name_);
  MInputMethod *im = find_im_by_name(name);
  if (im) {
    ic_array[id].mic = minput_create_ic(im, NULL);
  }
  ic_array[id].old_candidates = NULL;
  ic_array[id].new_candidates = NULL;
  free(name);
  return uim_scm_make_int(id);
}

static uim_lisp
free_id(uim_lisp id_)
{
  int id = uim_scm_c_int(id_);
  if (id < max_input_contexts) {
    struct ic_ *ic = &ic_array[id];
    if (ic->mic) {
      minput_destroy_ic(ic->mic);
      ic->mic = NULL;
    }
  }
  return uim_scm_f();
}

static uim_lisp
push_symbol_key(uim_lisp id_, uim_lisp key_)
{
  int id = uim_scm_c_int(id_);
  MSymbol key;
  MInputContext *ic = ic_array[id].mic;
  key = msymbol(uim_scm_c_str(key_));
  if (key == Mnil) {
    return uim_scm_t();
  }

  if(minput_filter(ic, key, NULL)== 1) {
    return uim_scm_t();
  } else {
    return uim_scm_f();
  }
}

static uim_lisp
get_result(uim_lisp id_)
{
  MText *produced;
  char *commit_string;
  int consumed;
  int id = uim_scm_c_int(id_);
  MInputContext *ic = ic_array[id].mic;
  uim_lisp  consumed_, commit_string_;

  produced  = mtext();

  consumed  = minput_lookup(ic, NULL, NULL, produced);

  if (consumed == -1) {
    consumed_ = uim_scm_f();
  } else {
    consumed_ = uim_scm_t();
  }

  commit_string = convert_mtext2str(produced);
  m17n_object_unref(produced);
  commit_string_ = uim_scm_make_str(commit_string);
  free(commit_string);

  return uim_scm_cons(consumed_, commit_string_);
}

static uim_lisp
commit(uim_lisp id_)
{
  int id = uim_scm_c_int(id_);
  MInputContext *ic = ic_array[id].mic;

/* To avoid a bug of m17n-lib */
  ic->candidate_to   = 0;
  return uim_scm_f();
}

static uim_lisp
candidate_showp(uim_lisp id_)
{
  int id = uim_scm_c_int(id_);
  MInputContext *ic = ic_array[id].mic;

  if (ic->candidate_show == 1) {
    return uim_scm_t();
  }
  return uim_scm_f();
}

static int
calc_cands_num(int id)
{
  int result = 0;
  MPlist *group; 
  MInputContext *ic = ic_array[id].mic;

  if (!ic || !ic->candidate_list)
    return 0;

  group = ic->candidate_list;

  while (mplist_value(group) != Mnil) {
    if (mplist_key(group) == Mtext) {
      for (; mplist_key(group) != Mnil; group = mplist_next(group)) {
        result += mtext_len(mplist_value(group));
      }
    } else {
      for (; mplist_key(group) != Mnil; group = mplist_next(group)) {
        result += mplist_length(mplist_value(group));
      }
    }
  }
  return result;
}

static void
old_cands_free(char **old_cands)
{
  int i = 0;
  if (old_cands) {
    for (i=0; old_cands[i]; i++) {
      free(old_cands[i]);
    }
    free(old_cands);
  }
}

static uim_lisp
fill_new_candidates(uim_lisp id_)
{
  MText *produced = NULL; /* Quiet gcc */
  MPlist *group;
  MPlist *elm;
  int i;
  char *buf = NULL; /* Quiet gcc */
  int id = uim_scm_c_int(id_);
  MInputContext *ic = ic_array[id].mic;
  int cands_num = calc_cands_num(id);
  char **new_cands;
  uim_lisp buf_;

  if (!ic || !ic->candidate_list)
    return uim_scm_f();

  group = ic->candidate_list;

  old_cands_free(ic_array[id].old_candidates);
  ic_array[id].old_candidates = ic_array[id].new_candidates;

  new_cands = malloc (cands_num * sizeof(char *) + 2);

  if (mplist_key(group) == Mtext) {

    for (i=0; mplist_key(group) != Mnil; group = mplist_next(group)) {
      int j;
      for (j=0; j < mtext_len(mplist_value(group)); j++, i++) {
          produced = mtext();
          mtext_cat_char(produced, mtext_ref_char(mplist_value(group), j));
          new_cands[i] = convert_mtext2str(produced);
          m17n_object_unref(produced);
      }
    }
  } else {
    for (i=0; mplist_key(group) != Mnil; group = mplist_next(group)) {

      for (elm = mplist_value(group); mplist_key(elm) != Mnil; elm = mplist_next(elm),i++) {
	produced = mplist_value(elm);
	new_cands[i] = convert_mtext2str(produced);
      }
    }
  }
  new_cands[i] = NULL;

  ic_array[id].new_candidates = new_cands;
  ic_array[id].nr_candidates = i;
  
  return uim_scm_t();

  if (!buf) {
    return uim_scm_make_str("");
  } else {
    buf_ = uim_scm_make_str(buf);
    free(buf);
    return buf_;
  }
}

static uim_bool
same_candidatesp(const char **old, const char **new)
{
  int i;
  if (!old)
    return UIM_FALSE;

  for (i=0; old[i] && new[i]; i++) {
    if (strcmp(old[i], new[i]) != 0) {
      return UIM_FALSE;
    }
  }
  return UIM_TRUE;
}

static uim_lisp
candidates_changedp(uim_lisp id_)
{
  int id = uim_scm_c_int(id_);
  char **old_cands = ic_array[id].old_candidates;
  char **new_cands = ic_array[id].new_candidates;

  if (!same_candidatesp(old_cands, new_cands)) {
    return uim_scm_t();
  }
  return uim_scm_f();
}

static uim_lisp
get_nr_candidates(uim_lisp id_)
{
  int id = uim_scm_c_int(id_);
  return uim_scm_make_int(calc_cands_num(id));
}

static uim_lisp
get_nth_candidate(uim_lisp id_, uim_lisp nth_)
{
  int id = uim_scm_c_int(id_);
  int nth = uim_scm_c_int(nth_);
  int nr = ic_array[id].nr_candidates;
  
  if (nr >= nth) {
    return uim_scm_make_str(ic_array[id].new_candidates[nth]);
  } else {
    return uim_scm_make_str("");
  }
}

static uim_lisp
get_candidate_index(uim_lisp id_)
{
  int id = uim_scm_c_int(id_);
  MInputContext *ic = ic_array[id].mic;
  return uim_scm_make_int(ic->candidate_index);
}

/* Utility function */
static char *
m17nlib_utf8_find_next_char(const char *p)
{
  if (*p) {
    for (++p; (*p & 0xc0) == 0x80; ++p)
      ;
  }
  return (char *)p;
}

void
uim_plugin_instance_init(void)
{
  uim_scm_init_subr_0("m17nlib-lib-init", init_m17nlib);
  uim_scm_init_subr_0("m17nlib-lib-nr-input-methods", get_nr_input_methods);
  uim_scm_init_subr_1("m17nlib-lib-nth-input-method-lang", get_input_method_lang);
  uim_scm_init_subr_1("m17nlib-lib-nth-input-method-name", get_input_method_name);
  uim_scm_init_subr_1("m17nlib-lib-alloc-context", alloc_id);
  uim_scm_init_subr_1("m17nlib-lib-free-context", free_id);
  uim_scm_init_subr_2("m17nlib-lib-push-symbol-key", push_symbol_key);
  uim_scm_init_subr_1("m17nlib-lib-compose-mode?", compose_modep);
  uim_scm_init_subr_1("m17nlib-lib-preedit-changed?", preedit_changedp);
  uim_scm_init_subr_1("m17nlib-lib-get-left-of-cursor",     get_left_of_cursor);
  uim_scm_init_subr_1("m17nlib-lib-get-right-of-cursor",    get_right_of_cursor);
  uim_scm_init_subr_1("m17nlib-lib-get-left-of-candidate",  get_left_of_candidate);
  uim_scm_init_subr_1("m17nlib-lib-get-selected-candidate", get_selected_candidate);
  uim_scm_init_subr_1("m17nlib-lib-get-right-of-candidate", get_right_of_candidate);
  uim_scm_init_subr_1("m17nlib-lib-get-result", get_result);
  uim_scm_init_subr_1("m17nlib-lib-commit", commit);
  uim_scm_init_subr_1("m17nlib-lib-candidate-show?", candidate_showp);
  uim_scm_init_subr_1("m17nlib-lib-fill-new-candidates!", fill_new_candidates);
  uim_scm_init_subr_1("m17nlib-lib-candidates-changed?", candidates_changedp);
  uim_scm_init_subr_1("m17nlib-lib-get-nr-candidates", get_nr_candidates);
  uim_scm_init_subr_2("m17nlib-lib-get-nth-candidate", get_nth_candidate);
  uim_scm_init_subr_1("m17nlib-lib-get-candidate-index", get_candidate_index);
}

void
uim_plugin_instance_quit(void)
{
  if (converter) {
    mconv_free_converter(converter);
    converter = NULL;
  }
  if (m17nlib_ok) {
    M17N_FINI();
    m17nlib_ok = 0;
    free(im_array);
    free(ic_array);
  }
}
