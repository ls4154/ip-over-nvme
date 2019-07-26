/*
 * Macro set for conditional 
 *
 * ex)
 *  #define MYCONDFUNC(COND,FUNC)
 *      __CONDDEF(COND, do_something();)
 *      __CONDNDEF(COND, do_another();)
 *          ...
 *  #define COND1 TRUE
 *  #define COND2 FALSE
 *          ...
 *  MYCONDFUNC(COND1) //do_something() will be excuted
 *          ...
 *  MYCONDFUNC(COND2) //do_another() will be excuted
 */
#pragma once
#ifndef __DBG_MACRO__SET__
#define __DBG_MACRO__SET__

#define __MACRO_CONCAT__(x,y) x##y
#define __MACRO_CONCAT2__(x,y) __MACRO_CONCAT__(x,y)
#define __MACRO_DEF_COND_TRUE(...) __VA_ARGS__;
#define __MACRO_DEF_COND_FALSE(...) ;
#define __MACRO_NDEF_COND_TRUE(...) ;
#define __MACRO_NDEF_COND_FALSE(...) __VA_ARGS__;

#define __CONDDEF(BOOL,...) \
    __MACRO_CONCAT2__(__MACRO_DEF_COND_,BOOL)(__VA_ARGS__)
#define __CONDNDEF(BOOL,...) \
    __MACRO_CONCAT2__(__MACRO_NDEF_COND_,BOOL)(__VA_ARGS__)

#endif