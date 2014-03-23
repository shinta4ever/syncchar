// MetaTM Project
// File Name: allochist.cc
//
// Description: 
//
// Operating Systems & Architecture Group
// University of Texas at Austin - Department of Computer Sciences
// Copyright 2007. All Rights Reserved.
// See LICENSE file for license terms.

#include "simulator.h"
#include "allochist.h"

#ifdef LOG_ALLOC

#include <malloc.h>

using namespace std;

typedef struct {
   unsigned int mark;
   size_t sz;
   void *caller[NUM_FRAMES];
} alloc_info;

// oh that's right I'm writing my own splay tree
typedef struct _s_node {
   void *val;
   unsigned long long sz;
   struct _s_node *parent;
   struct _s_node *left;
   struct _s_node *right;
} s_node;

static s_node **callers = NULL;

static s_node *_s_find(s_node *s_root, void *val) {
   s_node *ret = s_root;
   s_node *last = NULL;
   while(ret != NULL) {
      last = ret;
      if(ret->val == val) {
         return ret;
      }
      else if(val < ret->val) {
         ret = ret->left;
      }
      else {
         ret = ret->right;
      }
   }

   if(ret == NULL) {
      return last;
   }
   else {
      return ret;
   }
}

static void _s_splay(s_node *n) {
   while(n->parent != NULL) {
      s_node *p = n->parent;
      s_node *g = n->parent->parent;

      if(g == NULL) {
         if(n == p->left) {

            p->left = n->right;
            if(n->right != NULL) {
               n->right->parent = p;
            }

            n->right = p;
            p->parent = n;
         }
         else {
            p->right = n->left;
            if(n->left != NULL) {
               n->left->parent = p;
            }

            n->left = p;
            p->parent = n;
         }

         n->parent = NULL;
      }
      else {
         // zig-zag
         if(p->left == n && g->right == p) {
            g->right = n->left;
            if(n->left != NULL) {
               n->left->parent = g;
            }

            p->left = n->right;
            if(n->right != NULL) {
               n->right->parent = p;
            }

            n->left = g;
            n->right = p;

            n->parent = g->parent;
            if(g->parent && g->parent->left == g) {
               g->parent->left = n;
            }
            else if(g->parent && g->parent->right == g) {
               g->parent->right = n;
            }

            p->parent = g->parent = n;
         }
         else if(p->right == n && g->left == p) {
            g->left = n->right;
            if(n->right != NULL) {
               n->right->parent = g;
            }

            p->right = n->left;
            if(n->left != NULL) {
               n->left->parent = p;
            }

            n->right = g;
            n->left = p;

            n->parent = g->parent;
            if(g->parent && g->parent->left == g) {
               g->parent->left = n;
            }
            else if(g->parent && g->parent->right == g) {
               g->parent->right = n;
            }

            p->parent = g->parent = n;
         }

         // zig-zig
         else if(p->left == n && g->left == p) {
            g->left = p->right;
            if(g->left) {
               g->left->parent = g;
            }

            p->left = n->right;
            if(p->left) {
               p->left->parent = p;
            }

            p->right = g;
            n->right = p;

            n->parent = g->parent;
            if(g->parent && g->parent->left == g) {
               g->parent->left = n;
            }
            else if(g->parent && g->parent->right == g) {
               g->parent->right = n;
            }
            
            p->parent = n;
            g->parent = p;
         }
         else if(p->right == n && g->right == p) {
            g->right = p->left;
            if(g->right) {
               g->right->parent = g;
            }

            p->right = n->left;
            if(p->right) {
               p->right->parent = p;
            }

            p->left = g;
            n->left = p;

            n->parent = g->parent;
            if(g->parent && g->parent->left == g) {
               g->parent->left = n;
            }
            else if(g->parent && g->parent->right == g) {
               g->parent->right = n;
            }
            
            p->parent = n;
            g->parent = p;
         }
      }
   }
}

static void s_update_add(s_node **s_root, void *val, size_t sz) {
   s_node *n = _s_find(*s_root, val);
   if(!n) {
      n = (s_node*)malloc(sizeof(s_node));
      n->val = val;
      n->sz = sz;
      n->left = NULL;
      n->right = NULL;
      n->parent = NULL;
   }
   else if(n->val == val) {
      n->sz += sz;
   }
   else {
      s_node *n2 = (s_node*)malloc(sizeof(s_node));
      n2->val = val;
      n2->sz = sz;
      n2->left = NULL;
      n2->right = NULL;

      if(val < n->val) {
         n->left = n2;
         n2->parent = n;
      }
      else {
         n->right = n2;
         n2->parent = n;
      }

      n = n2;
   }

   _s_splay(n);
   *s_root = n;
}

static void s_update_sub(s_node **s_root, void *val, size_t sz) {
   s_node *n = _s_find(*s_root, val);
   if(!n || n->val != val) {
      cerr << "Free from unknown\n" << endl;
      return;
   }

   n->sz -= sz;
   _s_splay(n);
   *s_root = n;
}

static inline void *get_ebp() {
   void *ret;
   asm ("movl %%ebp, %0" : "=g"(ret) : : );
   return ret;
}

static inline void *get_prev_ebp(void *ebp) {
   void *ret;
   asm ("movl (%1), %0" : "=r"(ret) : "r"(ebp) : );
   return ret;
}

static inline void *get_caller(void *ebp) {
   void *ret;
   asm ("movl 4(%1), %0" : "=r"(ret) : "r"(ebp) : );
   return ret;
}

static inline void get_callers(alloc_info *a) {
   void *ebp = get_ebp();
   int i;
   for(i = 0; i < NUM_FRAMES; i++) {
      a->caller[i] = get_caller(ebp);
      ebp = get_prev_ebp(ebp);
   }
}

static void _traverse_inorder(s_node *n) {
   if(n == NULL) {
      return;
   }
   _traverse_inorder(n->left);
   cout << n->val << " " << n->sz << endl;
   _traverse_inorder(n->right);
}

void dump_alloc_hist() {
   int i;
   for(i = 0; i < NUM_FRAMES; i++) {
      cout << "CALLER INFO(" << i << "):" << endl;
      _traverse_inorder(callers[i]);
   }
   malloc_stats();
}

void *operator new(size_t sz) {
   alloc_info *ret = (alloc_info*)malloc(sz + sizeof(alloc_info));
   if(ret == NULL) {
      throw std::bad_alloc();
   }

   if(callers == NULL) {
      callers = (s_node**)calloc(NUM_FRAMES, sizeof(s_node*));
      int i;
      for(i = 0; i < NUM_FRAMES; i++) {
         callers[i] = NULL;
      }
   }

   ret->mark = 0xdeadcafe;
   ret->sz = sz;
   get_callers(ret);

   int i;
   for(i = 0; i < NUM_FRAMES; i++) {
      s_update_add(&callers[i], ret->caller[i], sz);
   }

   return ret + 1;
}

void operator delete(void *x) {
   if(x == NULL) {
      return;
   }
   
   alloc_info *mem = ((alloc_info*)x) - 1;

   int i;
   for(i = 0; i < NUM_FRAMES; i++) {
      s_update_sub(&callers[i], mem->caller[i], mem->sz);
   }
   
   free(mem);
}

#endif
