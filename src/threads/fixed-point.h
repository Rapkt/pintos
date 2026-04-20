//
// Created by rapkt on 4/17/26.
//

#ifndef OS_PINTOS_FIXED_POINT_H
#define OS_PINTOS_FIXED_POINT_H
#define F (1 << 14) // F = 2^14
#define INT_TO_FIXED(n) ((n) *(F))
#define FIXED_TO_INT_ZERO(n) ((n) /(F))
#define FIXED_TO_INT_NEAREST(n) (((n) >= 0)? (((n)+F/2)/F) : (((n)-F/2)/F))
#define ADD_FIX(x,y) ((x)+(y))
#define SUB_FIX(x,y) ((x)-(y))
#define ADD_NOR(x,n) ((x) + ((n) * (F)))
#define SUB_NOR(x,n) ((x) - ((n)*(F)))
#define MUL_FIX(x,y) ((((int64_t)(x)) * (y)/(F)))
#define MUL_NOR(x,n) ((x) * (n))
#define DIV_FIX(x,n) ((((int64_t)(x)) * (F) / (n)))
#define DIV_NOR(x,n) ((x) / (n))
#endif //OS_PINTOS_FIXED_POINT_H