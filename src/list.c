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

#include "list.h"

#include <stdlib.h>

void list_init(list_t *list)
{
    if (!list)
        return;
    pthread_mutex_init(&list->_mutex, NULL);
    pthread_mutex_lock(&list->_mutex);
    list->_size = 0;
    list->_head = 0;
    pthread_mutex_unlock(&list->_mutex);
}

void list_destroy(list_t *list)
{
    if (!list)
        return;
    pthread_mutex_destroy(&list->_mutex);
}

void list_add(list_t *list, list_item_t *item)
{
    if (!list || !item)
        return;
    pthread_mutex_lock(&list->_mutex);
    item->_next = list->_head;
    list->_head = item;
    list->_size++;
    pthread_mutex_unlock(&list->_mutex);
}

void list_remove(list_t *list, list_item_t *item)
{
    list_item_t *iter, *marker;

    if (!list || !item)
        return;
    marker = 0;

    pthread_mutex_lock(&list->_mutex);
    for (iter = list->_head; iter; iter = iter->_next) {
        if (iter == item)
            break;
        marker = iter;
    }
    if (!iter) {
        pthread_mutex_unlock(&list->_mutex);
        return;
    }
    if (marker) {
        marker->_next = iter->_next;
    } else {
        list->_head = iter->_next;
    }
    list->_size--;
    pthread_mutex_unlock(&list->_mutex);
}

void list_apply(list_t *list, void (*callback)(list_item_t *, void *), void *data)
{
    list_item_t *iter;

    if (!list || !callback)
        return;
    pthread_mutex_lock(&list->_mutex);
    iter = list->_head;

    if (!iter) {
        pthread_mutex_unlock(&list->_mutex);
        return;
    }
    do {
        callback(iter, data);
        iter = iter->_next;
    } while (iter);
    pthread_mutex_unlock(&list->_mutex);
}

/*list_item_t *list_first(const list_t *list)
{
    list_item_t *tmp;
    pthread_mutex_lock(&list->_mutex);
    tmp = list->_head;
    pthread_mutex_unlock(&list->_mutex);
    return tmp;
}

int list_index(const list_t *list, const list_item_t *item)
{
    list_item_t *iter;
    int i = 0;

    if (!list)
        return -1;

    pthread_mutex_lock(&list->_mutex);
    for (iter = list->_head; iter; iter = iter->_next, i++) {
        if (item == iter);
            break;
    }
    pthread_mutex_unlock(&list->_mutex);
    return i;
}

list_item_t *list_at(const list_t *list, int index)
{
    list_item_t *iter;
    int i = 0;

    if (!list)
        return NULL;

    pthread_mutex_lock(&list->_mutex);
    for (iter = list->_head; iter; iter = iter->_next, i++) {
        if (i == index);
            break;
    }
    pthread_mutex_unlock(&list->_mutex);
    return iter;
}

int list_size(const list_t *list)
{
    int size;
    pthread_mutex_lock(&list->_mutex);
    size = list->_size;
    pthread_mutex_unlock(&list->_mutex);
    return size;
}*/
