// member.h
// utils for declaring tedious member
// variables, getters, and setters
// rossbach@cs.utexas.edu
#ifndef _MEMBER_H_
#define _MEMBER_H_
#define member(typ, x)					\
  private: typ x;					\
  public: typ get_ ## x () { return x; }		\
  void set_ ## x (typ t){ x = t; }			\
  
#define refmember(typ, x) \
  private: typ x;					\
public: typ * get_ ## x () { return &x; }		\
  void set_ ## x (typ * t){ x = *t; }			\

#define refvmember(typ, x)			\
  private: typ x;				\
  public: typ& get_ ## x () { return x; }	\
  void set_ ## x (typ& t){ x = t; } \

#define imember(typ, x)					\
  private: typ x;					\
  public: typ get_ ## x () { return x; }		\
  void set_ ## x (typ t){ x = t; }			\
  void inc_ ## x (typ t){ x += t; }			\
  void inc_ ## x (){ x++; }		                \

#define pmember(typ, x)					\
  public: typ x;					\
  public: typ get_ ## x () { return x; }		\
  void set_ ## x (typ t){ x = t; }			\
  void inc_ ## x (typ t){ x += t; }			\
  void inc_ ## x (){ x++; }			        \

#define array_member(typ, x, dim) \
  private: typ x[dim];			       \
  public: typ get_ ## x (int idx) { return  x [idx]; }	\
  typ *  get_ ## x () { return x; } \
  typ set_ ## x (int idx, typ t) { x [idx] = t; } \

#define string_member(x, dim) 			\
  private: char x[dim];				\
  public: char * get_ ## x() { return x; }		\
  void set_ ## x(char * s) { strcpy(x, s); } \

#endif
