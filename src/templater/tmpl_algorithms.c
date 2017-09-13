/**
 * \author Michal Režňák
 * \date   8/28/17
 */

#include <stdbool.h>
#include <string.h>
#include "tmpl_algorithms.h"
#include "tmpl_common.h"
#include "tmpl_template.h"

// TEMPLATE VECTOR
vectm_t *
vectm_create()
{
    vectm_t *res = malloc(sizeof(vectm_t));
    if (res == NULL) {
        return NULL;
    }

    res->fields = malloc(sizeof(struct vectm_fields)); // TODO How many?
    if (res->fields == NULL) {
        return NULL;
    }

    res->end  = 0;
    res->used = 1;
    res->global_die_time = 0;
    return res;
}

size_t
vectm_get_count(const vectm_t *vec)
{
    return vec->end;
}

vectm_t *
vectm_copy(vectm_t *vec)
{
    vectm_t *res = malloc(sizeof(vectm_t));
    res->end  = vec->end;
    res->used = vec->used;

    res->fields = malloc(vec->used*sizeof(struct vectm_fields));
    memcpy(res->fields, vec->fields, vec->end*sizeof(struct vectm_fields));
    return res;
}

ipx_tmpl_template_t *
vectm_get_template(const vectm_t* vec, size_t index)
{
    return vec->fields[index].templates;
}

void
vectm_set_index(ipx_tmpl_t *tmpl, vectm_t *vec, size_t index, ipx_tmpl_template_t *src)
{
    vec->fields[index].templates = src;

    if (vec->fields[index].die_time == 0) {
        vec->fields[index].die_time = src->time.end + tmpl->life.time;
    }
}

void
vectm_set_die_time(vectm_t *vec, size_t index, uint64_t time)
{
    vec->fields[index].die_time = time;
}

uint64_t
vectm_get_die_time(vectm_t *vec, size_t index)
{
    return vec->fields[index].die_time;
}

uint64_t
vectm_get_global_die_time(vectm_t *vec)
{
    return vec->global_die_time;
}

bool
vectm_resize(vectm_t *vec)
{
    if (vec->end >= vec->used) {
        if (vec->used == 0) {
            vec->used = 4;
        }

        vec->used *= 2;
        vec->fields = realloc(vec->fields, vec->used*sizeof(struct vectm_fields));
        if (vec->fields == NULL) {
            return false;
        }
    } // TODO check too big vector

    return true;
}

ssize_t
vectm_find_index(const vectm_t *vec, uint16_t id)
{
    int first = 0;
    int last = (int) vec->end -1;
    int middle = (first + last) /2;
    struct ipx_tmpl_template *tmp;

    while (first <= last) {
        tmp = vec->fields[middle].templates;
        if (tmp->id == id) {
            return middle;
        }
        if (tmp->id < id) {
            first = middle +1;
        } else {
            last = middle -1;
        }
        middle = (first + last) /2;
    }

    return -1;
}

ipx_tmpl_template_t *
vectm_find(const vectm_t *vec, uint16_t id)
{
    ssize_t index = vectm_find_index(vec, id);
    if (index < 0) {
        return NULL;
    }

    return vec->fields[index].templates;
}

int
tmpl_cmp(const void* first, const void* second)
{
    const struct vectm_fields *a = first;
    const struct vectm_fields *b = second;

    return (a->templates->id - b->templates->id);
}

void
vectm_sort(vectm_t *vec)
{
    qsort(vec->fields, vec->end, sizeof(struct vectm_fields), tmpl_cmp);
}

bool
vectm_remove(vectm_t *vec, size_t index)
{
    free(vec->fields[index].templates->raw.data);
    free(vec->fields[index].templates);

    vec->fields[index] = vec->fields[vec->end -1];
    vec->end--;
    vectm_sort(vec);
    return true;
}

uint64_t
set_die_time(ipx_tmpl_t *tmpl, ipx_tmpl_template_t *src)
{
    if (tmpl->flag.care_time) {
        return src->time.first + tmpl->life.time;
    }

    uint64_t die_time = src->time.end;
    if (die_time != 0) {
        die_time += tmpl->life.time;
    }
    return die_time;
}

void
vectm_add(ipx_tmpl_t *tmpl, vectm_t *vec, ipx_tmpl_template_t *res)
{
    if (!vectm_resize(vec)) {
        return;
    }

    struct vectm_fields *field = &vec->fields[vec->end];
    field->templates           = res;
    field->die_time            = set_die_time(tmpl, res);
    if (field->die_time < vec->global_die_time || vec->global_die_time == 0) {
        vec->global_die_time = field->die_time;
    }
    vec->end++;
}

void
vectm_destroy(vectm_t *vec)
{
    free(vec->fields);
    free(vec);
}


// GARBAGE

/** Garbage                                                            */
struct garbage {
    /** Vector of templates                                            */
    struct temps {
        size_t                end;    /**< One after the last element   */
        size_t                used;   /**< Size of the allocated memory */
        ipx_tmpl_template_t **fields; /**< templates                    */
    } temps;

    /** Vector of template indexes                                      */
    struct indexes {
        size_t                end;    /**< One after the last element   */
        size_t                used;   /**< Size of the allocated memory */
        uint16_t             *fields;  /**< Indexes                      */
    } index;

    ipx_tmpl_t *tmpl;
    ipx_tmpl_t *snapshot; /**< One before the snapshot that should be removed */
};

struct garbage *
tmpl_garbage_create(ipx_tmpl_t *tmpl)
{
    struct garbage *res = malloc(sizeof(struct garbage));
    res->temps.end      = 0;
    res->temps.used     = 0;
    res->temps.fields   = NULL;
    res->index.end    = 0;
    res->index.used   = 0;
    res->index.fields = NULL;
    res->tmpl         = tmpl;
    return res;
}

/**
 * \brief Resize garbage if there isn't any space for another template
 * \param[in,out] gar Garbage
 * \return True if no error, False if memory error
 */
bool
tmpl_garbage_template_resize(struct garbage* gar)
{
    if (gar->temps.end >= gar->temps.used) {
        if (gar->temps.used == 0) {
            gar->temps.used = 4;
        }
        gar->temps.used *= 2;
        gar->temps.fields = realloc(gar->temps.fields, gar->temps.used*sizeof(ipx_tmpl_template_t));
        if (gar->temps.fields == NULL) {
            return false;
        }
    }
    return true;
}

/**
 * \brief Resize garbage if there isn't any space for another template
 * \param[in,out] gar Garbage
 * \return True if no error, False if memory error
 */
bool
tmpl_garbage_template_index_resize(struct garbage* gar)
{
    if (gar->index.end >= gar->index.used) {
        if (gar->index.used == 0) {
            gar->index.used = 4;
        }
        gar->index.used *= 2;
        gar->index.fields = realloc(gar->index.fields, gar->index.used*sizeof(ipx_tmpl_template_t));
        if (gar->index.fields == NULL) {
            return false;
        }
    }
    return true;
}

bool
tmpl_garbage_template_add(struct garbage *gar, ipx_tmpl_template_t *tmp)
{
    if (!tmpl_garbage_template_resize(gar)) {
        return false;
    }

    gar->temps.fields[gar->temps.end] = tmp;
    gar->temps.end++;
    return true;
}

bool
tmpl_garbage_template_index_add(struct garbage *gar, uint16_t index)
{
    if (!tmpl_garbage_template_index_resize(gar)) {
        return false;
    }

    gar->index.fields[gar->index.end] = index;
    gar->index.end++;
    return true;
}

void
tmpl_garbage_snapshot_add(struct garbage *gar, ipx_tmpl_t *snapshot)
{
    gar->snapshot = snapshot;
}

/**
 * \brief Remove all templates except \p src template
 * \param[in] src Template
 */
void
templates_remove_previous(ipx_tmpl_template_t *src)
{
    ipx_tmpl_template_t *tmp = src->next;
    ipx_tmpl_template_t *rem = NULL;
    while(tmp != NULL) {
        rem = tmp;
        tmp = tmp->next;
        template_destroy(rem);
    }
}

/**
 * \brief Remove all templates from a garbage
 * \param[in] gar Garbage
 */
void
garbage_templates_remove(garbage_t *gar)
{
    for (size_t i = 0; i < gar->temps.end; ++i) {
        templates_remove_previous(gar->temps.fields[i]);
        gar->temps.fields[i]->next = NULL;
    }
    free(gar->temps.fields);
}

void
garbage_indexes_remove(garbage_t *gar)
{
    for (size_t i = 0; i < gar->index.end; ++i) {
        templates_remove_previous(gar->tmpl->templates->fields[i].templates);
        vectm_remove(gar->tmpl->templates, gar->index.fields[i]);
    }
    free(gar->index.fields);
}

void
garbage_snapshots_remove(garbage_t *gar)
{
    const ipx_tmpl_t *snap = gar->snapshot->snapshot;
    const ipx_tmpl_t *rem;
    while(snap != NULL) {
        rem = snap;
        snap = snap->snapshot;

        vectm_destroy(rem->templates);
        free((void*) rem);
    }
    gar->snapshot->snapshot = NULL;
}

void
tmpl_garbage_destroy(garbage_t *gar)
{
    garbage_templates_remove(gar);
    garbage_indexes_remove(gar);
    garbage_snapshots_remove(gar);
    free(gar);
}

// BINARY TREE
struct node{
    ipx_tmpl_template_t *key;
    struct node *left;
    struct node *right;
};

node_t *
create_node(ipx_tmpl_template_t *key)
{
    node_t *res = malloc(sizeof(node_t));
    if (res == NULL) {
        return NULL;
    }
    res->key   = key;
    res->left  = NULL;
    res->right = NULL;
    return res;
}

void
tmpl_tree_add(node_t **leaf, ipx_tmpl_template_t *key, bool is_left) {
    node_t *res = create_node(key);
    if (res == NULL) {
        return;
    }

    if (is_left) {
        (*leaf)->left = res;
    } else {
        (*leaf)->right = res;
    }
}

const node_t *
tmpl_tree_get(node_t **leaf, ipx_tmpl_template_t *key) {
    if (*leaf == NULL) {
        *leaf = create_node(key);
        return NULL;
    }

    if ((*leaf)->key == key) {
        return *leaf;
    }
    else if ((*leaf)->key < key) {
        if ((*leaf)->left != NULL) {
            return tmpl_tree_get(&(*leaf)->left, key);
        }
        tmpl_tree_add(leaf, key, true);
    }
    else {
        if ((*leaf)->right != NULL) {
            return tmpl_tree_get(&(*leaf)->right, key);
        }
        tmpl_tree_add(leaf, key, false);
    }
    return NULL;
}

void
destroy(node_t *leaf)
{
    if (leaf->left != NULL) {
        destroy(leaf->left);
    }
    if (leaf->right != NULL) {
        destroy(leaf->right);
    }

    free(leaf);
}

void
tmpl_tree_destroy(node_t *leaf)
{
    if (leaf == NULL) {
        return;
    }

    destroy(leaf);
}
