/*
    Copyright 2008  Sascha Pelicke <sasch.pe@gmx.de>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
    IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
    NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
    THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef LIST_H
#define LIST_H

#include <pthread.h>

typedef struct list_item {
    void *data;
    struct list_item *_next;
} list_item_t;

typedef struct list {
    int _size;
    struct list_item *_head;
    pthread_mutex_t _mutex;
} list_t;

void list_init(list_t *list);
void list_destroy(list_t *list);

void list_add(list_t *list, list_item_t *item);
void list_remove(list_t *list, list_item_t *item);
void list_apply(list_t *list, void (*callback)(list_item_t *, void *), void *data);
/*list_item_t *list_first(const list_t *list);
int list_index(const list_t *list, const list_item_t *item);
list_item_t *list_at(const list_t *list, int index);
int list_size(const list_t *list);*/

#endif

