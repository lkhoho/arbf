//
//  Common.h
//  arbf-test
//
//  Created by Ke Liu on 2/19/16.
//  Copyright (c) 2016 Ke Liu. All rights reserved.
//

#ifndef __arbf_test_Common_h__
#define __arbf_test_Common_h__

#define SQ(x) ((x) * (x))

typedef struct _vertex {
    double x;
    double y;
    double z;
    double intensity;
} Vertex;

typedef struct _face {
    int a;
    int b;
    int c;
    double intensity;
} Face;

typedef struct _edge {
    int a;
    int b;
    double intensity;
} Edge;

enum BasisType { MQ, IMQ, Gaussian, TPS }; // type of basis functions

#endif /* defined(__arbf_test_Common_h__) */
